#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Copyright (c) 2026 The X Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Orphan processing resumes across turns and stays responsive.

A peer sends many transactions that each spend several outputs of a
parent the node has not seen yet, then sends the parent. Every orphan
is accepted into the mempool. A second peer completes ping/pong while
that work is still in progress.
"""

import os
import time

from test_framework.mininode import (
    CTransaction,
    MsgTx,
    NetworkThread,
    NodeConn,
    NodeConnCB,
    from_hex,
)
from test_framework.test_framework import RavenTestFramework
from test_framework.util import assert_equal, p2p_port

# Enough dependents that resolving them takes more than one turn, and
# few enough to stay under the orphan map and descendant limits.
ORPHAN_COUNT = 32
INPUTS_PER_ORPHAN = 8


class P2POrphanProcessingTest(RavenTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = False
        self.num_nodes = 1
        self.extra_args = [[
            "-maxorphantx=200",
            "-par=1",
            "-limitdescendantcount=500",
            "-limitdescendantsize=2000",
            "-limitancestorcount=500",
            "-limitancestorsize=2000",
        ]]

    def setup_network(self):
        self.setup_nodes()

    def _debug_log(self):
        path = os.path.join(self.nodes[0].datadir, "regtest", "debug.log")
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()

    def _make_parent_and_orphans(self, node):
        n_out = ORPHAN_COUNT * INPUTS_PER_ORPHAN
        utxo = None
        for entry in node.listunspent():
            if entry["amount"] >= n_out + 1:
                utxo = entry
                break
        assert utxo is not None, "no mature output large enough for the parent"

        outputs = {}
        for _ in range(n_out):
            outputs[node.getnewaddress()] = 1
        raw_parent = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], outputs)
        signed_parent = node.signrawtransaction(raw_parent)
        assert_equal(signed_parent["complete"], True)
        parent_hex = signed_parent["hex"]
        parent = node.decoderawtransaction(parent_hex)
        parent_txid = parent["txid"]
        assert_equal(len(parent["vout"]), n_out)

        prevouts = [{
            "txid": parent_txid,
            "vout": v["n"],
            "scriptPubKey": v["scriptPubKey"]["hex"],
            "amount": v["value"],
        } for v in parent["vout"]]

        orphan_hexes = []
        orphan_txids = []
        for i in range(ORPHAN_COUNT):
            group = prevouts[i * INPUTS_PER_ORPHAN:(i + 1) * INPUTS_PER_ORPHAN]
            raw_orphan = node.createrawtransaction(
                [{"txid": p["txid"], "vout": p["vout"]} for p in group],
                {node.getnewaddress(): INPUTS_PER_ORPHAN - 1})
            signed_orphan = node.signrawtransaction(raw_orphan, group)
            assert_equal(signed_orphan["complete"], True)
            orphan_hexes.append(signed_orphan["hex"])
            orphan_txids.append(node.decoderawtransaction(signed_orphan["hex"])["txid"])
        return parent_hex, parent_txid, orphan_hexes, orphan_txids

    def _send_tx(self, peer, hex_tx):
        tx = from_hex(CTransaction(), hex_tx)
        peer.send_message(MsgTx(tx))

    def run_test(self):
        node = self.nodes[0]
        self.log.info("Building a parent and %d orphans (%d inputs each)" % (
            ORPHAN_COUNT, INPUTS_PER_ORPHAN))
        parent_hex, parent_txid, orphan_hexes, orphan_txids = self._make_parent_and_orphans(node)

        peer_a = NodeConnCB()
        peer_b = NodeConnCB()
        conn_a = NodeConn('127.0.0.1', p2p_port(0), node, peer_a)
        conn_b = NodeConn('127.0.0.1', p2p_port(0), node, peer_b)
        peer_a.add_connection(conn_a)
        peer_b.add_connection(conn_b)
        NetworkThread().start()
        peer_a.wait_for_verack()
        peer_b.wait_for_verack()

        self.log.info("Peer A sending orphans before the parent")
        for hex_tx in orphan_hexes:
            self._send_tx(peer_a, hex_tx)
        peer_a.sync_with_ping()

        log_after_orphans = self._debug_log()
        stored = log_after_orphans.count("stored orphan tx")
        assert stored >= ORPHAN_COUNT, "expected orphans to be stored, saw %d" % stored
        assert "mapOrphan overflow" not in log_after_orphans

        # Baseline so later log checks ignore work from this setup.
        log_mark = len(log_after_orphans)

        self.log.info("Peer A sending the parent; peer B pinging immediately")
        self._send_tx(peer_a, parent_hex)
        started = time.time()
        peer_b.sync_with_ping(timeout=3)
        elapsed = time.time() - started
        self.log.info("Peer B ping/pong returned in %.3f seconds" % elapsed)
        assert elapsed < 3, "second peer did not get a pong while orphans were processed"

        deadline = time.time() + 60
        missing = list(orphan_txids)
        while time.time() < deadline:
            mempool = set(node.getrawmempool())
            missing = [txid for txid in orphan_txids if txid not in mempool]
            if parent_txid in mempool and not missing:
                break
            time.sleep(0.05)
        else:
            tail = self._debug_log()[-4000:]
            raise AssertionError(
                "orphans were not all accepted; missing %d (parent in mempool=%s)\n%s" % (
                    len(missing), parent_txid in set(node.getrawmempool()), tail))

        mempool = set(node.getrawmempool())
        assert parent_txid in mempool
        for txid in orphan_txids:
            assert txid in mempool, "orphan %s was not accepted" % txid
        assert_equal(len(orphan_txids), len(set(orphan_txids)))

        tail = self._debug_log()[log_mark:]
        accepted = [i for i, line in enumerate(tail.splitlines()) if "accepted orphan tx" in line]
        pings = [i for i, line in enumerate(tail.splitlines()) if "received: ping" in line]
        self.log.info("accepted orphan log lines: %d; ping log lines after parent: %d" % (
            len(accepted), len(pings)))
        assert len(accepted) >= ORPHAN_COUNT, "expected each orphan to be accepted (%d lines)" % len(accepted)
        assert "removed orphan tx" not in tail
        # A ping from the other peer is handled between orphan validations,
        # which is the interruptible behavior.
        interleaved = any(
            any(a < p for a in accepted) and any(a > p for a in accepted)
            for p in pings)
        assert interleaved, "ping was not handled while orphan work was still in progress"

        peer_b.sync_with_ping(timeout=3)
        self.log.info("All %d orphans are in the mempool and the node still answers ping" % ORPHAN_COUNT)


if __name__ == '__main__':
    P2POrphanProcessingTest().main()
