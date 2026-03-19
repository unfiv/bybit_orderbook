* Build/onboard issues:
- OpenSSL is not plugged via defauld Conan config nor on cmake level (find_package + target_link_libs), for a person to be onboarded it will hurt
- main.cpp: unistd.h doesn't exist on Windows, the code isn't cross-platform
- client.cpp: compilation error at "std::format(R"({"op" as mixing formatting with raw string
- client.cpp: C1128 error, one more issue disabling project to work out of the box
- client.cpp: no buffer consuming afer message reading within on_read, causes json exceptions on the second reading
- bookbuilder.cpp: main loop in applyDelta goes out of the bounds because of "auto idx"

* Infrastructure
- CMakeLists.txt: we pin C++ standard via "CMAKE_CXX_STANDARD 23" for compilation, but package manager (Conan) has no mechanics to validate packages he downloads/build uses the same standard, we may face ABI issues and hardly possible to detect runtime problems
- CMakeLists.txt: libraries versions are not pinned, each new CD and product will be a lottery during building and, which is more dangerous, in production; moreover Conan says specific version, but cmake doesn't check it (we are free to have packages already installed or use another package manager)
- src/CMakeLists.txt: "file(GLOB_RECURSE SRC \*.cpp)" integrates sources implicitly, better to keep everything explicitly, more work, but more control and project/solution regeneration
- src/CMakeLists.txt: for a small projects better to use one cmake file

* Logic issues:
- we use messages amount as input to our program and receive no more messages than this amount, while task description has no constraints description, we assume the program should work forever
- client.cpp: ping_timer uses 1 minute delay, while bybit's documentation says we need to send heartbit packet every 20 seconds
- client.cpp: ping_timer uses wrong message (subscription), documentation says about special one
- client.cpp: if num_messages_ goes to 0, we close socket but keep timer alive (should be canceled in order not to use closed socket), program loops forever
- client.cpp: num_messages_ considers all types of messages received (technical, data)
- client.cpp: ping_timer wait time is set only once in constructor, it won't call on_timer second time, need to setup each time
- client.cpp: protocol answers are not considered during message's handling (if 'subscribe' message is received with non 'success' we will still run expecting data)
- client.cpp: no proper program termination in case something goes wrong (for example, if we pass wrong topic and subscription isn't established, we will loop forver)
- client.cpp: handle 'a' data (ask) as 'buy', while it should be 'sell' (same for bids)
- client.cpp: 'snapshot' message is not handled (it should clear current book state)
- client.cpp: we convert string to double and will loose precision as well as compare operations may fail (we need to switch to fixed point arithmetics and implement additional logics)
- client.cpp: we do not consider sequence and updateId fields ('seq'/'u'), but according to the docs orderbook is valid only if we receive data sequentially (no gaps, no reverse order), so we should invalidate book in that case, u=1 also should reset the book
- client.cpp: we do not monitor 'cts' from the protocol as well, should be considered to avoid big lags
- constants.hpp: "constexpr const char \* const" generates duplicates for every cpp it's attached ("inline constexpr std::string_view" avoids it
- bookbuilder.hpp: hardcoded type for book parameter, if depth constant changes, we need to fix type manually (should use the same template as for book)
- book.hpp: empty compares nan with nan, which is always false, algorithms work wrong

* Performance issues
- main.cpp: no need to "std::stoi(argv[1])" in order to parse (it creates/destroys std::string as stoi() wants it)
- client.hpp: handleMessage make a string copy as an input param
- client.cpp: ping/heartbeat mechanics should consider receiving market data and avoid ping-ponging, no need to block socket and use network resources if logically we are in alive state
- client.cpp: buffers_to_string creates new string each time in heap (no reason to expect short string for SSO), we should use string_view or reuse preallocated string, it's a hot path
- client.cpp: stod works with locale/allocations, we should avoid it (for example in favor of from_chars)
- client.cpp: kv[0].get<nlohmann::json::string_t>() creates string, while we can avoid it with going directly to the buffer (get_ref)
- client.cpp: in case of high frequency data receiving, i/o operation "print" may become a bottleneck (we may print every X times and/or flush right after or move to another thread)
- nlohmann library for json parsing should be non-optimal, research for alternatives
- client.cpp: topic_ and subscribe_msg_ can be easily created compile time and managed within constants (even manually)
- bookbuilder.cpp: if we've found px_cmp==0, we have unnecessary instructions before check if we remove the element (first check and remove, else push new data)
- bookbuilder.cpp: search algorithm has O(N) complexity as runs over all elements, but we don't need it as we already have sorted data in both arrays (binary search + technical tuning needed)
- book.hpp: PriceLevel's methods clear and empty are not marked as noexcept (ease inlining, better branching, more compiler optimizations, which is critical as this class is used in hot path)
- bookbuilder.hpp: applyDelta isn't marked as noexcept, which may reduce compiler's optimizations

* Architectural issues:
- client.hpp: Client is a god-object, handling connection, bybit data and logics; substitute any part is a real pain (for example: use local data instead of network, change bybit to another market with different data format. etc); should be decoupled and spread across layers (transport (network/file) <=> market-specific adapters <=> pure logics)
- client.hpp: Client class actually implements specific market, but is named as a common one; we should split interface (base class) and implementation, bybit folder should contain implementation only for a specific market — connector and book builder, both book and client are implementation-independent
- client.cpp: we use strands for creating async objects and operations, we guarantee they will go one after another, but if we start event loop on different threads and expect everything works (as we use strands!) we will have problems, because each object uses own strand (and ws object is used in on_ping_timer of ping_timer and on_read of ws) and data theys share (for example ws) is not thread safe; solution is to create one strand and use it
- client.cpp: consider parsing data (on_read) within a separate thread to avoid blocking if data stream is expected to be huge (same for printing)

* Other issues:
- main.cpp: no index check accessing argv[1], UB
- main.cpp: standard main signature should return int
- main.cpp: no exception catching on the top level, we want to shut down gracefully in the worst case and have something in log with the details
- main.cpp: signal processing is safer to do with handling signals as regular events in loop, current code "signal(SIGINT, signalHandler)" which stops loop doesn't allow async operations to be shut gracefully, resources may leak, logic problems may arise
- main.cpp: messages should be unsigned, as we don't expect negative amount
- main.cpp: "std::make_shared<bybit::Client>(ioc, ctx, messages)->run();" destroys the object right after ";", so all async callbacks (on_resolve first) will deal with destroyed object
- main.cpp: no need to make ioс global variable
- client.cpp: as a part of the previous problem we are passing this to async code, while created object may be destroyed from outside, we should allow all the callbacks to share Client object (enable_shared_from_this mechanics and passing share_from_this instead of raw this)
- client.cpp: prefix operation is generally better "--num_messages_"
- client.cpp: no error code check for net::error::operation_aborted case inside on_error handler, it will spam messages for a "regular" sigint exit with ctrl+c
- client.cpp: if broken message comes from bybit, most probably handling message will cause an exception; better to skip one dataset with exception handling there
- client.cpp: mechanics to keep connection alive is called "ping", while it should be more  high level logical "heartbeat" (bybit documentation states it as well)
- client.cpp: no checks if 'a' or 'b' exists, exception if try to get with []
- client.cpp: output is messy, better to format for same width
- client.cpp: code duplication when parsing 'a'/'b' prices
- client.hpp: "using namespace boost::asio::ip" makes all file's includes use this namespace, potential conflicts
- client.hpp: namespaces' aliases also will be included if client.hpp is included somewhere
- client.hpp: Bookbuilder\* \_builder member, it will be deleted twice in destructor if parent object is copied; no reason to store it as pointer at all, it should be behind book as class member (but decouple everything is still the best option)
- inconsistent naming (class member "book" vs other class' member "book_", MAX_NUM_LEVELS vs depth), some variables/functions are named in full, some not ("amt")
- inconsistent brackets usage, line lengths, newlines
- no "#pragma once" directive for all the headers
- constants.hpp: no check if depth is valid, we will know it only after full run and receiving an answer (static_assert will help to check complile-time)
- constants.hpp: global constants (should be in namespace like the rest of the code)
- a lot of heavy templates make building pretty long, should use any of existing solutions to speed up dev phase
- bookbuilder.hpp: pointer to book shows builder just don't hold resource and just observes it, but reference is better technically (forbid creation with null)
- lack of minimal unit testing, integration tests are very welcome as well, stress test stand with ability to check system would be ideal addition
- contradictory ssl usage: we set verify callback as host_name_verification and later set verify mode as verify_none
- use specific revisions for plugged packages/libraries use revisions (if exist), also consider options for more deterministic control (for example, use static libs instead of dynamic)
- cmakelistst.txt: release mode is not set up