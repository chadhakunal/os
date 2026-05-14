# Todos


  - Look into updating the virtio driver to use the blocking infrastructure
  - Fix the page fault stuff, see if the print stuff is only happening with the stacktest, 
    - Try only taking one page and printing the whole thing
    - Then 2
    - Then 3 -> This or 2 should break it since it will be increasing past the stack vma
    - disabling for now to test if page fault is issue, and see if we get a regular page fault now
    - 2. Guard page on the kernel stack
        Map a non-present page at the bottom of each process's kernel stack. If your fault handler itself overflows, you get a clean fault rather than silent corruption/infinite recursion.

  - add in pipe support
