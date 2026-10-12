# Research references

- [Linux seccomp BPF documentation](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html) describes syscall filtering used for local child processes.
- [NEST benchmark examples](https://nest-simulator.readthedocs.io/en/stable/auto_examples/brette_et_al_2007/index.html) and [Brian 2 CUBA example](https://brian2.readthedocs.io/en/2.5.3/examples/CUBA.html) show external workloads. No numerical comparison is asserted without running aligned models on the same hardware.

- [Intel Labs neuromorphic research](https://www.intel.com/content/www/us/en/research/neuromorphic-computing.html) describes programmable neuron and synapse research on Loihi 2. NeuroMantle does not execute on Loihi.
- [SpiNNaker project](https://doi.org/10.1109/Jproc.2014.2304638) describes lightweight multicast routing at neuromorphic scale. This project's single-threaded schedule is a different determinism choice.
- [Brian 2 STDP example](https://brian2.readthedocs.io/en/2.5.0.3/examples/synapses.STDP.html) provides an independent reference for spike-timing plasticity modeling. NeuroMantle implements its explicitly documented discrete integer rule.
- [NEST models](https://nest-simulator.readthedocs.io/en/main/models/) catalog broader neuron and synapse models. No parity or performance superiority is claimed.
