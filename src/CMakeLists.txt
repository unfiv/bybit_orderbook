add_executable(${PROJECT_NAME} 
    main.cpp 
    bybit/client.cpp
)

target_link_libraries(${PROJECT_NAME}
    boost::boost
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    )
