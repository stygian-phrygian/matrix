set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)

# if we haven't already, add imported target library
if(NOT TARGET LibCurses::LibCurses)
    add_library(LibCurses::LibCurses SHARED IMPORTED)
    set_target_properties(
        LibCurses::LibCurses
        PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIRS}"
        IMPORTED_LOCATION "${CURSES_LIBRARY}")
endif()

