"""One thing that is wrong with a game, said so a 10-year-old can fix it."""

from dataclasses import asdict, dataclass


@dataclass(frozen=True)
class Problem:
    code: str          # short stable name, for tools and tests
    file: str          # file name inside the game folder, e.g. "levels.txt"
    line: int | None   # 1-based line number, or None when no line applies
    message: str       # what is wrong, in plain words
    hint: str          # how to fix it

    def to_dict(self):
        return asdict(self)

    def text(self, folder=""):
        if self.file == ".":
            where = folder or "."
        elif folder and not self.file.startswith("engine/"):
            where = f"{folder}/{self.file}"
        else:
            where = self.file
        if self.line is not None:
            where += f":{self.line}"
        return f"{where}: {self.message}\n    Fix: {self.hint}"
