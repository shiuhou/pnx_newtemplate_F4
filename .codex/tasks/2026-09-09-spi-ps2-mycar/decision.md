# Decision

Use the already hardware-verified shared BSP/modules commits rather than copy
their implementation into the vehicle repository. The vehicle repository owns
only product composition: SPI backend selection, runtime source injection,
receiver-safe clock selection, matching CAN timing, and regression contracts.

The control adapter and mode router remain untouched so transport migration
cannot silently alter operator semantics. Parent publication is deferred until
the two dependency commits are available from remotes.
