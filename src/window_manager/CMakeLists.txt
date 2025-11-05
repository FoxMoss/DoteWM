add_executable(noko
  main.cc
  ../protobuf/windowmanager.pb.cc
)

set(CMAKE_CXX_STANDARD 17)

target_compile_options(noko PRIVATE -std=c++20 -g)

find_package(X11 REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
target_link_libraries(noko ${X11_LIBRARIES} Xext GL GLU GLEW Xcomposite Xfixes gRPC::grpc++)
