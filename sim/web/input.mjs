export const KEY_TURNS = Object.freeze({
  a: [0, 4], d: [0, -4], j: [1, 4], l: [1, -4],
});
export const KEY_BUTTONS = Object.freeze({ s: 0, k: 1 });

export function angleDelta(previous, current) {
  return Math.atan2(Math.sin(current - previous), Math.cos(current - previous));
}

export function angleAt(x, y, rect) {
  return Math.atan2(y - rect.top - rect.height / 2, x - rect.left - rect.width / 2);
}

export class Rotation {
  remainder = 0;
  move(radians) {
    this.remainder += radians * 80 / (2 * Math.PI);
    const transitions = Math.trunc(this.remainder + Math.sign(this.remainder) * 1e-10);
    this.remainder -= transitions;
    return transitions;
  }
}

export function arrowTilt(keys) {
  return [
    Number(keys.has("ArrowRight")) - Number(keys.has("ArrowLeft")),
    Number(keys.has("ArrowDown")) - Number(keys.has("ArrowUp")),
  ];
}

export function validateDevice(frame) {
  return frame && ["leftTotal", "rightTotal"].every(k => Number.isInteger(frame[k]) && frame[k] >= -2147483648 && frame[k] <= 2147483647)
    && ["leftDown", "rightDown"].every(k => typeof frame[k] === "boolean")
    && ["tiltX", "tiltY"].every(k => Number.isFinite(frame[k]) && Math.abs(frame[k]) <= 1);
}

export class DeviceInput {
  previous = null;
  apply(frame) {
    if (!validateDevice(frame)) throw new Error("Invalid console input frame");
    const turns = this.previous
      ? [(frame.leftTotal - this.previous.leftTotal) | 0, (frame.rightTotal - this.previous.rightTotal) | 0]
      : [0, 0];
    this.previous = frame;
    return { turns, buttons: [frame.leftDown, frame.rightDown], tilt: [frame.tiltX, frame.tiltY] };
  }
  reset() { this.previous = null; }
}
