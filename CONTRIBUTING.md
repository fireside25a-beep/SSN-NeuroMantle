# Contributing

Keep the C11 neuron transition and the C++ exact audit in agreement. Any change to neuron or synapse semantics should update `docs/MODEL.md`, the independent Python oracle and the admission/replay tests. Run `make test`, `make companions`, `make -C vendor/devcli test`, `make vanta-check` and the sanitizer target in a supported environment. Preserve deterministic tie rules, event ordering and canonical receipt fields or introduce an explicit new receipt version.

Keep changes to imported components localized and document them in `docs/PROVENANCE.md`. Do not commit generated build directories or temporary receipts. Make no hardware or formal-proof claim without an executable gate and recorded evidence.
