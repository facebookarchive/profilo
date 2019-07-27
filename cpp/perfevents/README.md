#### What is `perfevents`?

`libperfevents` is a saner interface to the `perf_event_open` Linux API, specifically
built for attaching to your own process. `perfevents` is also a clean room
implementation and not tainted by `perf(1)`'s GPL license.

#### What is `perf_event_open`?

`perf_event_open(2)` is the interface into the Linux kernel's perf events
infrastructure. As such, it is also the mechanism behind `perf(1)`.

It is a complicated API - a single syscall controls about 30 different event types, multiple ways of reading or sampling data, as well as different ways of
attaching to a process or thread (let's call these tuples "modes of execution").
For example, you can directly `read(2)` some events but have to `mmap` a ring
buffer for other events.

On the other hand, `perf_event_open(2)` has one main user - `libperf` (and  
the `perf(1)` tool). As such, it has evolved hand-in-hand with `libperf` (which
is GPL licensed).

Due to this tight coupling, many of the officially documented modes of execution
don't actually work for an unprivileged process targeting itself. This situation
is made worse on Android where the kernel can be quite old and long-solved
bugs are still present.

`perfevents` hides all of these traps and complexity and aims to expose a simple and
sane API into `perf_event_open`, using only well-tested modes of execution.

#### Design

`perfevents` is designed around the following concepts:

`EventSpec`
  - High-level type of event that we want to capture.

`Event`
  - A concrete `(perf_event_attr, fd, cpu, tid)` tuple. Direct interface to the
    `perf_event_open` syscall and associated `ioctl` calls.

`SessionSpec`
  - A configuration object guiding Session on how to attach to the current
    process.

`Session`
  - Controls the attachment of the perf events and is the entry point to perfevents.

`AttachmentStrategy`
  - An algorithm to materialize a set of EventSpecs into actual Events.

`Reader`
  - An abstract type reading from a set of Events.

`RecordListener`
  - A listener notified for every record in the mapped ring buffers.

Concrete implementations:

`PerCoreAttachmentStrategy` (see below)

`FdPollReader` (to be used with PerCoreAttachmentStrategy)
  - Follows all already-mapped Events using `poll(2)` and outputs all events to
    the listener.

#### PerCoreAttachmentStrategy

`perf_event_open` supports event inheritance (copying the event
configuration to any new threads) only for per-cpu events.

Further, there is no way to do inherit + target-every-thread-in-the-app in a
single call. The only way to pull this off is to call `perf_event_open`
per-cpu, per-thread, per-event-that-you-want-inherited.

If we do this, we can set up the system to propagate event configs across every
thread, until our process dies. We don't need to enable any of these events,
but the fds must remain open. The earlier we do this, the fewer threads we have
to attach to, so the fewer fds we use.

Therefore the logic is this:

1. Read the event specs for every per-thread event we care about.
2. Attach to all current threads by polling /proc/self/task - attach until we
  converge or run too many iterations.
  * Initially attach with .disabled = 1
3. Select one event on every core to be the leader and `setOutput` all other
events to this leader.
4. `mmap` only the core leaders.
5. Use `FdPollReader` to poll only the core leaders (which get signalled on any
inherited event as well).

#### Clock notes

The timestamps in the samples are obtained via `perf_clock` which is
`local_time`, which in turn is a per-cpu monotonic clock. Therefore, if we
want to understand these precisely, we need per-cpu offsets from the
userspace monotonic clock.

#### Dev notes/questions

1. Are tracepoints per-thread?
  * Yes, sadly.
2. Is mmap/task process-global or per-thread?
  * `.task` is per-thread, can piggyback on something else
  * `.mmap`/`.mmap_data` are per-thread as well
3. Can we point the mmap/tracepoints to the same per-cpu buffers?
  * We can piggyback on a per-cpu event.
4. Does the open/mmap/close dance keep the fd around?
  * No, surprisingly.
5. Does a closed event fd still inherit across threads?
  * Yes!
  * Therefore, if fd exhaustion is a problem, we can close the fds and go into
    time-based polling mode instead, taking care not to read while
    `buffer->lock & 1` holds.
6. Does event forwarding work with closed fds?
  * Nope.
7. When A is forwarded to B, do I need to add A to the pollset or is B enough?
  * B is enough.
8. Does ioctl(DISABLE) signal the fd?
  * (Not empirical!)
  * The code in `events/core.c` deletes the event from the pmu,
  which stops it, which updates its `value` (the thing you `read(2)`) but it
  does not seem to trigger an overflow (what generates the sample/signals
  the fd).
  * (`__perf_event_disable` -> `event/group_sched_out` -> `pmu->del()` ->
   `cpu_clock_event_del` -> `cpu_clock_event_stop` -> `cpu_clock_event_update`)
  * I'll assume that it doesn't for now - this means that we still need a side
  channel to trigger a stop event for the Reader.

9. Does `CPU_CLOCK` require per-thread attachment?

10. Does `read(CPU_CLOCK)` give you the current time?
  * It seems to, yes, but the `value` of the counter is nowhere near the
  `timestamp` we see on the sample.
  * On Galaxy Duo, that timestamp is close to `CLOCK_MONOTONIC` though.

11. Does attaching per-core per-thread work when the core is offline? Does putting the core online resume the event?
  * It does. Toggling `/sys/devices/system/cpu/cpu${idx}/online` mid-run Just
  Works and scheduling events from the core become available/disappear with the
  changes to core online status.

#### TODOS

* Maybe handle ENFILE explicitly? Throw a specific exception, safely close the session?

* Explicitly check for the presence of `/proc/sys/kernel/perf_event_paranoid`
  * (This is the "official" way to check for the presence of the perf events infra)
  * Galaxy Young Duo doesn't have it
  * Galaxy Young Duo 2 doesn't have it
