add_library(hareflow)

target_include_directories(hareflow
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>
)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ProtonCpp REQUIRED)
target_link_libraries(hareflow
    PUBLIC
        Boost::boost
        fmt::fmt
        
    PRIVATE
        OpenSSL::SSL
        Proton::cpp
)

target_compile_definitions(hareflow
    PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:HAREFLOW_BUILDING_STATIC>
    PRIVATE
        $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x603>
        HAREFLOW_EXPORTS
)
target_compile_features(hareflow PUBLIC cxx_std_17)
target_compile_options(hareflow PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd4251 /wd4996>)

install(TARGETS hareflow
    EXPORT ${PROJECT_NAME}-targets
    COMPONENT runtime

    ARCHIVE DESTINATION ${INSTALL_LIBDIR}
    RUNTIME DESTINATION ${INSTALL_BINDIR}
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    RESOURCE DESTINATION ${INSTALL_SHAREDIR}
)

target_sources(hareflow
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include/hareflow.h
        
        ${PROJECT_SOURCE_DIR}/include/hareflow/client_parameters.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/client.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/codec.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/consumer_builder.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/consumer.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/environment_builder.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/environment.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/exceptions.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/io_context_holder.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/logging.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/message_builder.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/producer_builder.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/producer.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/qpid_proton_codec.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/stream_creator.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/types.h

        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/accumulator.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/background_scheduler.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/binary_buffer.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/binary_buffer.hpp
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/client_impl.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/client_impl.hpp
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/commands.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/connection.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/consumer_impl.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/environment_impl.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/internal_types.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/producer_impl.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/reschedulable_task.h
        ${PROJECT_SOURCE_DIR}/include/hareflow/detail/semaphore.h
        
        ${CMAKE_CURRENT_LIST_DIR}/accumulator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/background_scheduler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/binary_buffer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/client_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/client_parameters.cpp
        ${CMAKE_CURRENT_LIST_DIR}/codec.cpp
        ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
        ${CMAKE_CURRENT_LIST_DIR}/connection.cpp
        ${CMAKE_CURRENT_LIST_DIR}/consumer_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/consumer_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/environment_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/environment_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/exceptions.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io_context_holder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/logging.cpp
        ${CMAKE_CURRENT_LIST_DIR}/message_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/producer_builder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/producer_impl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/qpid_proton_codec.cpp
        ${CMAKE_CURRENT_LIST_DIR}/reschedulable_task.cpp
        ${CMAKE_CURRENT_LIST_DIR}/stream_creator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/types.cpp
)
