set(CMAKE_CXX_STANDARD 17)

set(INC
  ../include/
)

set(LIB
)

set(SRC
  main.cpp 
)

add_executable(swr ${SRC})

target_include_directories(swr PUBLIC ${INC})

target_link_libraries(swr PRIVATE ${LIB})
