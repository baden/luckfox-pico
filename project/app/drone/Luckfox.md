CONFIG_TCP_CONG_BBR:                                                                                                                                                                  │
  │                                                                                                                                                                                       │
  │                                                                                                                                                                                       │
  │ BBR (Bottleneck Bandwidth and RTT) TCP congestion control aims to                                                                                                                     │
  │ maximize network utilization and minimize queues. It builds an explicit                                                                                                               │
  │ model of the bottleneck delivery rate and path round-trip propagation                                                                                                                 │
  │ delay. It tolerates packet loss and delay unrelated to congestion. It                                                                                                                 │
  │ can operate over LAN, WAN, cellular, wifi, or cable modem links. It can                                                                                                               │
  │ coexist with flows that use loss-based congestion control, and can                                                                                                                    │
  │ operate with shallow buffers, deep buffers, bufferbloat, policers, or                                                                                                                 │
  │ AQM schemes that do not provide a delay signal. It requires the fq                                                                                                                    │
  │ ("Fair Queue") pacing packet scheduler.
