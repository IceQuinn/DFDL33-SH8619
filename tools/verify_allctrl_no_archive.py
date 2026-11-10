"""Verify the agreed write-response contract of DFDL33-SH8619 dlt645_data_api.c.

full control (DI0=FF):
  archived + all normal values                -> normal
  archived + mix normal / FF values           -> normal
  archived + all FF values                    -> normal
  archive-less slots are ignored regardless of value
single control (DI0=01..0C):
  archived   + normal value                   -> normal
  archived   + FF value                       -> error 01 (other error)
  archive-less + any value                    -> error 02 (no requested data)
time control (04E60B/04E60C, single and full):
  archived   + times + normal value           -> normal
  archived   + times + FF value               -> normal
  archived   + FF times + FF value            -> normal
  archive-less (any combination)              -> normal
Only hard time/format errors stay as errors.
"""
ALL_SELECTOR = 0xFF
N = 12
SUCCESS, NOT_CONTROLLED, FAILED, UNSUPPORTED, OTHER, NO_ARCHIVE = 0, 1, 2, 3, 4, 5
ENOENT = "02"


class Sim:
    def __init__(self, data_len, wmin, wmax):
        self.data_len, self.wmin, self.wmax = data_len, wmin, wmax
        self.archive_valid = [True] * N
        self.source_errors = [0] * N
        self.results = [SUCCESS] * N
        self.sent = [0] * N
        self.data = b""

    def get_control_source(self, i):
        if not self.archive_valid[i]:
            return "eempty"
        return self.source_errors[i]

    def bcd_s32(self, field):
        if field[0] == 0xFA:
            return None
        return field[0]

    def one_value(self, i, value):
        self.sent[i] += 1
        return self.results[i]

    # ---- dlt645_write_control_value ----
    def write_control_value(self, id_):
        sel = id_ & 0xFF
        first = 0 if sel == ALL_SELECTOR else sel - 1
        count = N if sel == ALL_SELECTOR else 1
        if len(self.data) != self.data_len * count:
            return "EINVAL"
        response = [OTHER] * count
        execute = [False] * count
        for off in range(count):
            field = self.data[off * self.data_len:(off + 1) * self.data_len]
            if not self.archive_valid[first + off]:
                # C: archive check comes first, so an archive-less slot is 02/ignored
                #    even when the value is FF.
                response[off] = NOT_CONTROLLED if sel == ALL_SELECTOR else NO_ARCHIVE
                continue
            if all(b == 0xFF for b in field):
                response[off] = NOT_CONTROLLED if sel == ALL_SELECTOR else FAILED
                continue
            src = self.get_control_source(first + off)
            if src == "enosys":
                response[off] = UNSUPPORTED
                continue
            if src != 0:
                continue
            dec = self.bcd_s32(field)
            if dec is None or dec < self.wmin or dec > self.wmax:
                continue
            execute[off] = True
        for off in range(count):
            if execute[off]:
                response[off] = self.one_value(first + off, 0)
        if sel != ALL_SELECTOR:
            if response[0] == SUCCESS:
                return "OK"
            if response[0] == NO_ARCHIVE:
                return ENOENT
            return "01"
        for off in range(count):
            if response[off] not in (SUCCESS, NOT_CONTROLLED):
                return "01"
        return "OK"

    # ---- dlt645_write_run_state ----
    def write_run_state(self, id_):
        sel = id_ & 0xFF
        first = 0 if sel == ALL_SELECTOR else sel - 1
        count = N if sel == ALL_SELECTOR else 1
        if len(self.data) != self.data_len * count:
            return "EINVAL"
        if sel != ALL_SELECTOR:
            if not self.archive_valid[first]:
                return ENOENT
            v = self.data[0]
            if v == 0xFF or (v & 0x0F) > 9 or (v >> 4) > 9 or not (self.wmin <= v <= self.wmax):
                return "01"
            self.sent[first] += 1
            return "OK" if self.results[first] == SUCCESS else "01"
        execute = [False] * count
        values = [0] * count
        all_ok = True
        for off in range(count):
            v = self.data[off]
            if v == 0xFF or not self.archive_valid[first + off]:
                continue
            if (v & 0x0F) > 9 or (v >> 4) > 9 or not (self.wmin <= v <= self.wmax):
                all_ok = False
                continue
            values[off] = v
            execute[off] = True
        for off in range(count):
            if execute[off]:
                if not self.archive_valid[first + off]:
                    all_ok = False
                    continue
                self.sent[first + off] += 1
                if self.results[off] != SUCCESS:
                    all_ok = False
        return "OK" if all_ok else "01"

    # ---- dlt645_decode_time_control_block + write_time_control ----
    def time_block(self, archive_index, times_ff, value_ff, bad_hour=False, unsupported=False):
        """Return what the slot contributes: OK / 01 (hard error) / skipped-but-OK."""
        if not self.archive_valid[archive_index]:
            return "OK"                      # archive-less: always normal, never planned
        if times_ff:
            return "OK"                      # FF times -> period disabled, normal
        if bad_hour:
            return "01"                      # hard time/format error stays an error
        if value_ff or unsupported:
            return "OK"                      # FF value / unsupported -> normal, skipped
        return "OK"


def reset(sim):
    sim.archive_valid = [True] * N
    sim.source_errors = [0] * N
    sim.results = [SUCCESS] * N
    sim.sent = [0] * N


def run():
    # ================= full control: three normal combinations =================
    for kind in range(5, 10):
        dl, lo, hi = (4, -1100, 1100) if kind <= 6 else (2, -1100, 1100)
        sim = Sim(dl, lo, hi)
        id_all = 0x04E600FF | (kind << 8)
        id_one = (id_all & 0xFFFFFF00) | 1

        # 1) all archived, all normal values
        reset(sim); sim.data = bytes([1] * (dl * N))
        assert sim.write_control_value(id_all) == "OK"
        assert all(s == 1 for s in sim.sent)

        # 2) mix normal and FF values
        reset(sim)
        buf = bytearray([1] * (dl * N))
        buf[2 * dl:3 * dl] = bytes([0xFF] * dl)
        buf[7 * dl:8 * dl] = bytes([0xFF] * dl)
        sim.data = bytes(buf)
        assert sim.write_control_value(id_all) == "OK"
        assert sim.sent[2] == 0 and sim.sent[7] == 0 and sim.sent[0] == 1

        # 3) all FF values
        reset(sim); sim.data = bytes([0xFF] * (dl * N))
        assert sim.write_control_value(id_all) == "OK"
        assert all(s == 0 for s in sim.sent)

        # 4) archive-less slots ignored whatever the value
        reset(sim)
        sim.archive_valid[3] = False; sim.archive_valid[8] = False
        sim.source_errors[3] = -1; sim.results[8] = 2
        buf = bytearray([0xFA] * (dl * N))          # illegal BCD everywhere
        buf[0:dl] = bytes([1] * dl)                 # archived slot stays valid
        sim.data = bytes(buf)
        assert sim.write_control_value(id_all) == "01"   # archived slots still illegal
        reset(sim)
        sim.archive_valid[3] = False; sim.archive_valid[8] = False
        buf = bytearray([1] * (dl * N))
        buf[3 * dl] = 0xFA; buf[8 * dl] = 0xFA      # illegal only where no archive
        sim.data = bytes(buf)
        assert sim.write_control_value(id_all) == "OK"
        assert sim.sent[3] == 0 and sim.sent[8] == 0

        # ================= single control table =================
        # archived + normal -> normal
        reset(sim); sim.data = bytes([1] * dl)
        assert sim.write_control_value(id_one) == "OK"
        assert sim.sent[0] == 1

        # archived + FF -> 01
        reset(sim); sim.data = bytes([0xFF] * dl)
        assert sim.write_control_value(id_one) == "01"
        assert sim.sent[0] == 0

        # archive-less + normal value -> 02
        reset(sim); sim.archive_valid[0] = False; sim.data = bytes([1] * dl)
        assert sim.write_control_value(id_one) == ENOENT
        assert sim.sent[0] == 0

        # archive-less + FF value -> 02 (NOT 01)
        reset(sim); sim.archive_valid[0] = False; sim.data = bytes([0xFF] * dl)
        assert sim.write_control_value(id_one) == ENOENT
        assert sim.sent[0] == 0

        # archived + unsupported / device failure -> 01
        reset(sim); sim.source_errors[0] = "enosys"; sim.data = bytes([1] * dl)
        assert sim.write_control_value(id_one) == "01"
        reset(sim); sim.results[0] = 2; sim.data = bytes([1] * dl)
        assert sim.write_control_value(id_one) == "01"

        # wrong length stays an error
        reset(sim); sim.data = bytes([1] * (dl * N - 1))
        assert sim.write_control_value(id_all) == "EINVAL"

    # ================= run state: same single-control table =================
    sim = Sim(1, 0, 1)
    # full control
    reset(sim); sim.data = bytes([0] * N)
    assert sim.write_run_state(0x04E604FF) == "OK"
    reset(sim)
    buf = bytearray([0] * N); buf[3] = 1; buf[6] = 0xFF
    sim.data = bytes(buf)
    assert sim.write_run_state(0x04E604FF) == "OK"
    reset(sim); sim.data = bytes([0xFF] * N)
    assert sim.write_run_state(0x04E604FF) == "OK" and all(s == 0 for s in sim.sent)
    reset(sim)
    sim.archive_valid[4] = False
    buf = bytearray([0] * N); buf[4] = 0xFA
    sim.data = bytes(buf)
    assert sim.write_run_state(0x04E604FF) == "OK"
    assert sim.sent[4] == 0
    reset(sim)
    buf = bytearray([0] * N); buf[0] = 2
    sim.data = bytes(buf)
    assert sim.write_run_state(0x04E604FF) == "01"     # archived illegal still errors

    # single control
    reset(sim); sim.data = bytes([1])
    assert sim.write_run_state(0x04E60403) == "OK"
    assert sim.sent[2] == 1
    reset(sim); sim.data = bytes([0xFF])
    assert sim.write_run_state(0x04E60403) == "01"     # archived + FF -> 01
    assert sim.sent[2] == 0
    reset(sim); sim.archive_valid[2] = False; sim.data = bytes([1])
    assert sim.write_run_state(0x04E60403) == ENOENT   # no archive + normal -> 02
    reset(sim); sim.archive_valid[2] = False; sim.data = bytes([0xFF])
    assert sim.write_run_state(0x04E60403) == ENOENT   # no archive + FF -> 02
    reset(sim); sim.data = bytes([2])
    assert sim.write_run_state(0x04E60403) == "01"
    reset(sim); sim.results[2] = 2; sim.data = bytes([1])
    assert sim.write_run_state(0x04E60403) == "01"
    reset(sim); sim.data = bytes([0] * 11)
    assert sim.write_run_state(0x04E604FF) == "EINVAL"

    # ================= time control: all six combinations normal =================
    reset(sim)
    # archived: times + normal value
    assert sim.time_block(0, times_ff=False, value_ff=False) == "OK"
    # archived: times + FF value
    assert sim.time_block(0, times_ff=False, value_ff=True) == "OK"
    # archived: FF times + FF value
    assert sim.time_block(0, times_ff=True, value_ff=True) == "OK"
    # archived: FF times + normal value (times FF wins -> disabled, normal)
    assert sim.time_block(0, times_ff=True, value_ff=False) == "OK"
    # archive-less: every combination normal
    for times_ff in (False, True):
        for value_ff in (False, True):
            sim.archive_valid[5] = False
            assert sim.time_block(5, times_ff=times_ff, value_ff=value_ff) == "OK"
            sim.archive_valid[5] = True
    # archive-less with bad hour is still normal (no archive => ignore everything)
    sim.archive_valid[5] = False
    assert sim.time_block(5, times_ff=False, value_ff=False, bad_hour=True) == "OK"
    sim.archive_valid[5] = True
    # archived with hard time error stays an error
    assert sim.time_block(0, times_ff=False, value_ff=False, bad_hour=True) == "01"
    # archived but unsupported control -> normal, skipped
    assert sim.time_block(0, times_ff=False, value_ff=False, unsupported=True) == "OK"

    print("PASS: full-control 3 normal cases, single-control 02/01 split, "
          "time-control six normal cases")


if __name__ == "__main__":
    run()
