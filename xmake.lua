add_rules('mode.debug', 'mode.release')

add_requires('argparse      3.1')
add_requires('spdlog        1.14.1')
add_requires('icu4c         75.1')
add_requires('nlohmann_json 3.11.3')

target('rkcfgtool')
    set_kind('binary')
    add_files('src/**.cpp')
    add_includedirs('src')
    set_warnings('all')
    set_languages('c99', 'c++20')
    add_packages('argparse', 'spdlog', 'icu4c', 'nlohmann_json')
    if is_mode('debug') then 
        add_defines('DEBUG')
    end
