add_executable(dotewm
  main.cc
  lodepng.cpp
  base64.cpp
)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

target_compile_options(dotewm PRIVATE -std=c++20 )

find_package(X11 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)
target_link_libraries(dotewm ${X11_LIBRARIES} Xext GL GLU GLEW Xcomposite Xfixes Xi nanomsg protobuf::libprotobuf absl::log_internal_check_op absl::log_internal_message nanomsg windowmanager_proto )

install(TARGETS dotewm
  RUNTIME DESTINATION bin
  COMPONENT applications
)

