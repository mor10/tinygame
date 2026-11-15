#!/usr/bin/env python3
"""First-try eval for the AI kit: does a model, given only the API card and two example carts,
write a cart that passes `tinygame-sim --check` on its first answer?

Usage:
  python3 ai/eval/run_eval.py --dry-run            # show what would be sent and a cost estimate
  python3 ai/eval/run_eval.py [--model claude-opus-5-5] [--effort medium] [--limit N] [--jobs 4]

Results go to ai/eval/results/<timestamp>/: each cart, its check output, and summary.json.
Needs the `anthropic` Python package and an API key or `ant auth login`. The scope's target is
that at least 80% of prompts pass on the first try.
"""
import argparse, concurrent.futures, json, pathlib, re, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parents[2]
SIM = ROOT / "sim" / "build" / "tinygame-sim"
EXAMPLES = ["console/carts/examples/rocks.lua", "console/carts/examples/snake.lua"]

INSTRUCTIONS = """You write games for TinyGame, a pocket console for kids. Reply with one complete
cart as a single ```lua code block, then at most two sentences on how to play.
Use only the API on the card below. Give the cart a cart table with title, tagline, api = 1,
fps = 30 and a 16x16 icon. Keep it short and readable, show the controls on screen in a few words,
and make it possible to lose and play again."""


def system_prompt():
    card = (ROOT / "docs" / "api-card.md").read_text()
    examples = "\n\n".join(f"Example cart `{p}`:\n```lua\n{(ROOT / p).read_text()}```" for p in EXAMPLES)
    return f"{INSTRUCTIONS}\n\n{card}\n\n{examples}"


def extract_lua(text):
    blocks = re.findall(r"```(?:lua)?\s*\n(.*?)```", text, re.S)
    return max(blocks, key=len) if blocks else None


def check(path):
    out = subprocess.run([str(SIM), "--check", str(path)], capture_output=True, text=True, timeout=120).stdout
    lines = [json.loads(l) for l in out.splitlines() if l.startswith('{"t":"check')]
    done = next((l for l in lines if l["t"] == "check_done"), {"ok": 0})
    return done, [l for l in lines if l["t"] == "check"]


def run_one(client, args, system, i, prompt, outdir):
    t0 = time.time()
    resp = client.beta.messages.create(
        model=args.model,
        max_tokens=16000,
        betas=["server-side-fallback-2026-07-01"],
        fallbacks="default",                       # a policy decline re-runs on a fallback model
        output_config={"effort": args.effort},
        system=[{"type": "text", "text": system, "cache_control": {"type": "ephemeral"}}],
        messages=[{"role": "user", "content": prompt}],
    )
    rec = {"i": i, "prompt": prompt, "stop_reason": resp.stop_reason, "seconds": round(time.time() - t0, 1),
           "input_tokens": resp.usage.input_tokens, "output_tokens": resp.usage.output_tokens,
           "cache_read": getattr(resp.usage, "cache_read_input_tokens", 0) or 0}
    if resp.stop_reason == "refusal":
        rec.update(ok=False, why="refused")
        return rec
    text = "".join(b.text for b in resp.content if b.type == "text")
    (outdir / f"{i:02d}.md").write_text(text)
    cart = extract_lua(text)
    if not cart:
        rec.update(ok=False, why="no lua block")
        return rec
    path = outdir / f"{i:02d}.lua"
    path.write_text(cart)
    done, problems = check(path)
    rec.update(ok=bool(done.get("ok")), errors=done.get("errors"), warnings=done.get("warnings"),
               problems=[p["msg"] for p in problems if p["level"] == "error"][:5])
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="claude-opus-5-5")
    ap.add_argument("--effort", default="medium", choices=["low", "medium", "high", "xhigh", "max"])
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    prompts = [l.strip() for l in (ROOT / "ai/eval/prompts.txt").read_text().splitlines()
               if l.strip() and not l.startswith("#")]
    if args.limit:
        prompts = prompts[: args.limit]
    system = system_prompt()
    if args.dry_run:
        approx = len(system) // 4
        print(f"{len(prompts)} prompts, system prompt about {approx} tokens (cached after the first call)")
        print(f"rough cost for {args.model}: up to ~{len(prompts) * 6000} output tokens including thinking")
        for p in prompts:
            print(" -", p)
        return
    if not SIM.exists():
        sys.exit(f"build the simulator first: cmake -S sim -B sim/build && cmake --build sim/build")

    import anthropic
    client = anthropic.Anthropic()
    outdir = ROOT / "ai/eval/results" / time.strftime("%Y%m%d-%H%M%S")
    outdir.mkdir(parents=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        futs = [pool.submit(run_one, client, args, system, i, p, outdir) for i, p in enumerate(prompts, 1)]
        for f in concurrent.futures.as_completed(futs):
            try:
                r = f.result()
            except anthropic.APIStatusError as e:
                r = {"ok": False, "why": f"API error {e.status_code}"}
            except anthropic.APIConnectionError:
                r = {"ok": False, "why": "connection error"}
            results.append(r)
            print(("PASS " if r.get("ok") else "FAIL ") + str(r.get("i", "?")).rjust(2), r.get("prompt", ""),
                  "" if r.get("ok") else r.get("problems") or r.get("why"), flush=True)
    passed = sum(1 for r in results if r.get("ok"))
    summary = {"model": args.model, "effort": args.effort, "prompts": len(prompts), "passed": passed,
               "rate": round(passed / max(1, len(prompts)), 3),
               "output_tokens": sum(r.get("output_tokens", 0) for r in results),
               "input_tokens": sum(r.get("input_tokens", 0) for r in results),
               "cache_read_tokens": sum(r.get("cache_read", 0) for r in results),
               "results": sorted(results, key=lambda r: r.get("i", 0))}
    (outdir / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"\n{passed}/{len(prompts)} passed first try ({summary['rate']:.0%}); results in {outdir}")


if __name__ == "__main__":
    main()
