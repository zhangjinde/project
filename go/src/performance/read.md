1. string to byte 涉及内存分配，性能消耗很大。
2. 短小的对象，复制的成本要大于堆上分配和回收操作。
3. map 的使用
4. defer 使用