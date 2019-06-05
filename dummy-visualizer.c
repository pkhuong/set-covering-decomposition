#include <stdio.h>

int main() {
  fprintf(stderr, "Build with --define gui=yes to enable visualisation.\n");
  fprintf(stderr, "The :visualizer target will then depend on glfw "
          "(sudo apt-get install libglfw3{,-dev} or libglfw3{-wayland,-dev}).\n");
  return 0;
}
