add_executable(noko
  main.cc
)

target_compile_options(noko PRIVATE -std=c++20)

find_package(X11 REQUIRED)
target_link_libraries(noko ${X11_LIBRARIES} Xext GL GLU GLEW Xcomposite Xfixes)
