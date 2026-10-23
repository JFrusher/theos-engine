# .theos File Format Specification v1.0

## 1. Overview
The `.theos` file format is a binary container designed to store logical theological arguments, premises, deductions, conclusions, and scriptural proof anchors.

## 2. Low-Level Binary Layout
Every `.theos` file consists of four contiguous segments:

### A. Header Block (Bytes 0–31)
- **Magic Number (4 bytes):** `0x54, 0x48, 0x45, 0x4F` ("THEO")
- **Version (2 bytes):** Major/Minor version (`0x0100` for v1.0)
- **Node Count (2 bytes):** Total number of logic nodes in the graph.
- **Axiom Count (2 bytes):** Total scriptural axiom references.
- **Offsets (20 bytes):**
  - `axiom_table_offset` (4 bytes, `uint32_t`)
  - `node_table_offset` (4 bytes, `uint32_t`)
  - `string_heap_offset` (4 bytes, `uint32_t`)
  - `reserved` (8 bytes)

### B. Scripture Axiom Table
A packed array of 32-bit unsigned integers representing scriptural references without string overhead.
- **Bit-Packing Layout (`uint32_t`):**
  - Bits 0–7 (8 bits): Book ID (1–66)
  - Bits 8–16 (9 bits): Chapter ID (1–512)
  - Bits 17–24 (8 bits): Start Verse (1–256)
  - Bits 25–31 (7 bits): End Verse offset (0–127)

### C. Graph Node Table
Fixed-width node records allowing $O(1)$ random access index lookup:
- `node_id` (`uint16_t`)
- `type` (`uint8_t`: 0x01 = Premise, 0x02 = Deduction, 0x03 = Conclusion)
- `dep_count` (`uint8_t`: number of incoming edges)
- `string_offset` (`uint32_t`: byte pointer into String Heap)
- `dep_node_ids` (`uint16_t[4]`: fixed array of parent dependency node IDs)
- `axiom_ids` (`uint16_t[2]`: attached scriptural axiom indices)

### D. String Heap
Contiguous null-terminated UTF-8 character arrays referenced by string offsets.

## 3. Validation Rules (DAG Graph Theory)
1. **Acyclic Constraint:** The logic graph must be a Directed Acyclic Graph. Cycles trigger a compilation abort.
2. **Orphan Check:** Every node of type `Conclusion` must resolve its dependency path back to at least one valid scriptural `Axiom`.