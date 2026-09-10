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
    assert slot_from_height(1, 1789197360) == 1789197360 // 60 + 1

    # Fair-launch subsidy: height 0 unpaid; 5000 XFER >> halvings; 64-shift cap.
    COIN = 10**8
    interval = 2100000
    def subsidy(h):
        if h < 1:
            return 0
        halvings = h // interval
        if halvings >= 64:
            return 0
        return (5000 * COIN) >> halvings
    assert subsidy(0) == 0
    assert subsidy(1) == 5000 * COIN
    assert subsidy(2099999) == 5000 * COIN
    assert subsidy(2100000) == 2500 * COIN
    lifetime = 0
    for k in range(64):
        sub = (5000 * COIN) >> k
        nblocks = (interval - 1) if k == 0 else interval
        lifetime += sub * nblocks
    # 20,999,994,999.727 XFER (integer >>= dust vs a clean 21B)
    assert lifetime == 2099999499972700000
    print("lottery_vector: ok")


if __name__ == "__main__":
    main()
