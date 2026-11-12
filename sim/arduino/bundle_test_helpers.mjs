import assert from "node:assert/strict";

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export function replaceMember(bundle, member, source) {
  const result = Buffer.from(bundle);
  const tableOffset = result.readUInt32LE(12);
  const tableLength = result.readUInt32LE(16);
  let position = tableOffset;
  for (let i = 0; i < result.readUInt16LE(8); i++) {
    const nameLength = result.readUInt16LE(position);
    const name = result.toString("ascii", position + 16, position + 16 + nameLength);
    if (name === member) {
      const offset = result.readUInt32LE(position + 4);
      const length = result.readUInt32LE(position + 8);
      const replacement = Buffer.from(source, "utf8");
      assert(replacement.length <= length);
      result.fill(0x20, offset, offset + length);
      replacement.copy(result, offset);
      result.writeUInt32LE(crc32(result.subarray(offset, offset + length)), position + 12);
      result.writeUInt32LE(crc32(result.subarray(tableOffset, tableOffset + tableLength)), 32);
      return result;
    }

    position += 16 + ((nameLength + 3) & ~3);
  }
  assert.fail(`fixture is missing ${member}`);
}

export function assertTransparentArea(pixels, width, left, top) {
  for (let y = 4; y < 28; y++) {
    for (let x = 4; x < 34; x++) {
      assert.equal(pixels[(top + y) * width + left + x], 0xf7bf);
    }
  }
}

export function assertClippedText(origin, clipped, width, left, top) {
  let visibleTextPixels = 0;
  for (let y = 0; y < 12; y++) {
    for (let x = 0; x < 40; x++) {
      const pixel = clipped[(top + y) * width + left + x];
      assert.equal(pixel, origin[(top + y + 4) * width + left + x + 8]);
      if (pixel !== 0xf7bf) visibleTextPixels++;
    }
    for (let x = 0; x < left; x++) assert.notEqual(clipped[(top + y) * width + x], 0xfa6c);
  }
  assert(visibleTextPixels > 0, "partially clipped text remains visible");
}
