#!/usr/bin/env python3
"""Sanity vectors for docs/LOTTERY.md (no C++ required)."""
import hashlib
import struct

SLOT_SECONDS = 60


def winner_count(height, interval):
    if interval <= 0:
        return 1
    return min(height // interval, 1023) + 1


def slot_from_height(height, genesis_time):
    if height <= 0:
        return genesis_time // SLOT_SECONDS
    return genesis_time // SLOT_SECONDS + height


def seed(prev_hex, slot):
    prev = bytes.fromhex(prev_hex)
    return hashlib.sha256(prev + struct.pack("<Q", slot)).digest()


def stream_u64(seed_bytes, counter):
    h = hashlib.sha256(seed_bytes + struct.pack("<I", counter)).digest()
    return int.from_bytes(h[:8], "little")


def select_winners(sorted_ids, seed_bytes, k):
    pool = list(sorted_ids)
    if not pool or k <= 0:
        return []
    if k >= len(pool):
        return pool
    winners = []
    for i in range(k):
        r = stream_u64(seed_bytes, i)
        remaining = len(pool) - i
        idx = i + (r % remaining)
        pool[i], pool[idx] = pool[idx], pool[i]
        winners.append(pool[i])
    return winners


def split_reward(total, winners):
    if winners <= 0 or total <= 0:
        return []
    base, rem = divmod(total, winners)
    parts = [base] * winners
    for i in range(rem):
        parts[i] += 1
    return parts


def main():
    assert winner_count(0, 2100000) == 1
    assert winner_count(2099999, 2100000) == 1
    assert winner_count(2100000, 2100000) == 2
    assert winner_count(4200000, 2100000) == 3
    assert winner_count(10, 150) == 1
    assert winner_count(150, 150) == 2

    parts = split_reward(5000 * 10**8, 3)
    assert sum(parts) == 5000 * 10**8
    assert parts[0] == parts[1] + 1 or parts[0] == parts[1]
    assert parts[0] >= parts[-1]

    ids = [bytes.fromhex(f"{i:040x}") for i in range(1, 6)]
    s = seed("11" * 32, 42)
    w1 = select_winners(ids, s, 2)
    w2 = select_winners(ids, s, 2)
    assert w1 == w2
    assert len(set(w1)) == 2
    assert slot_from_height(1, 1788825600) == 1788825600 // 60 + 1
    print("lottery_vector: ok")


if __name__ == "__main__":
    main()
