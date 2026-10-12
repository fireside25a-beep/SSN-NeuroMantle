# Spatial domain allocation

`stdlib/spatial.llr` is a trusted native runtime module wired to the `.vnt` compiler. It accepts an exactly 128-byte capability as a 1024-bit key. Sixteen little-endian 64-bit lanes are compared bit for bit. `spatial-new` creates a pool; `spatial-add topology pool key domain` snapshots one key and its destination domain. A pool holds at most one entry per configured memory domain, up to the runtime's 63-domain limit.

`spatial-select topology pool key` returns the index of the entry with minimum Hamming distance. Equal distances choose the first inserted entry. `spatial-alloc topology pool key bytes permissions` makes the same selection, calls the checked capability allocator for the chosen domain, and increments that entry's allocation counter. It returns an ordinary capability; free it with `mem-free`. `spatial-count pool index` reads the counter. `spatial-free pool` releases the pool mapping. All resources still require explicit cleanup.

The key capability requires read permission and visibility from the supplied topology. Its length must be exactly 128 bytes. This check runs before reading any key byte. Domain handles are validated before insertion and again during allocation. Empty pools, duplicate configured domains, invalid indices, exhausted slots, and invalid key lengths trap. A selected domain can be inaccessible to the calling topology after allocation; a later memory access still requires explicit topology visibility. This operation makes no physical accelerator dispatch claim.

## Example

```text
(let pool (spatial-new))
(spatial-add cpu pool key0 host)
(spatial-add cpu pool key1 other)
(let selected (spatial-select cpu pool query))
(let region (spatial-alloc cpu pool query 64 7))
(mem-free region)
(spatial-free pool)
```

The test `tests/positive/spatial.vnt` checks both matches, capacity-accounted allocations, counters, and a 512/512 tie. The runtime refusal tests check a 127-byte key, an empty pool, a duplicate domain, an unreadable key, and exhausted destination capacity; the negative compiler test checks static resource kinds. `scripts/test_spatial_oracle.py` independently computes Hamming distances with Python integer bit counts for 20 fixed queries over four domains and executes the emitted native program. The module is single-threaded, like the current generated process model.
