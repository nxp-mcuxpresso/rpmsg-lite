#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Resolve the multicore board-port root for the current board.
# frdmkw43 keeps its public multicore_examples port sources in the internal
# examples_int tree (so they are not visible in the public examples repo),
# even though the public board folder exists and board_root still points to
# examples/_boards. Redirect only that board; every other board keeps the
# standard board_root so their examples_int multicore_examples (e.g. NBU
# secondary ports) are unaffected.
if(board STREQUAL "frdmkw43")
    set(mc_board_root examples_int/_boards)
else()
    set(mc_board_root ${board_root})
endif()
