idf_component_register(SRCS "epdiy_main.c"
                       INCLUDE_DIRS ".")

# The generated firasans_*.h font tables carry the glyph characters (including
# bidirectional control chars) in trailing // comments. GCC 15's -Wbidi-chars
# flags these and IDF compiles with -Werror, so silence it for this component.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-bidi-chars)
