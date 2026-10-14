install(
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/bvi.svg
  DESTINATION
    share/icons/hicolor/scalable/apps
)

foreach(ICON_SIZE 16 32 48 64 128 256)
  install(
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/bvi${ICON_SIZE}.png
    DESTINATION
      share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps
    RENAME
      bvi.png
  )
endforeach()
