- [] roundedness is not clamped to half the smallest size
# Text Input
- [] text input no scrolling has been implemented check if a scrollview in the div is a nice idea or should be implemented separately
- [] text input pasting doesnt work probably a wayland part of issue check out wayland impelntation code
- [] text input ctrl+backspace doesnt work and double click to select word doesnt work on touchpad but works on mouse
- [] text input drag selection works only on same input field but not to other ones
# Text Rendering
- [x] text rendering: some interesting chars are not rendered example:
```c
  std::string displayString = textBuffer;
  if (config.isPassword && !textBuffer.empty()) {
    displayString.clear();
    size_t count = getCodepointCount(textBuffer);
    for (size_t i = 0; i < count; ++i) {
      displayString += "•";
    }
  }
  ```

  ```
```
- [x] text rendering: line height, word wrap anywhere, text align all dont work
- [] text rendering: multi language support is unverified
