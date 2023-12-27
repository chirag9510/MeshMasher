# CMakeList.txt : CMake project for MeshMasher, include source and define
# project specific logic here.
#

include_directories(${ASSIMP_INCLUDE_DIR} ${MESHOPTIMIZER_INCLUDE_DIR})

# Add source to this project's executable.
add_executable (MeshMasher "MeshMasher.cpp" "MeshMasher.h" "CQueue.h"  "CQueue.cpp" "stb_image.h" "Model.h"  "meshoptimizer.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MeshMasher PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(MeshMasher ${ASSIMP_LIBRARIES} ${MESHOPTIMIZER_LIBRARY})

# TODO: Add tests and install targets if needed.
