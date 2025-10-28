# Timewarp: learning OpenGL

Right mouse click project and open nuget package manager and install vcpkg<br>
or use git clone https://github.com/microsoft/vcpkg.git C:\cpp_src\vcpkg<br>
Inside Developer Powershell:<br>
- cd C:\cpp_src\vcpkg
- vcpkg install sdl2<br>
- vcpkg install sdl3<br>
- vcpkg integrate install<br>
- vcpkg install glad:x64-windows
- vcpkg install glew:x64-windows

Right mouse click project / properties:<br>
- Add to C++ / General / Additional Include Directories: C:\cpp_src\vcpkg\installed\x64-windows\include<br>
- Add to Linker / General / Additional library Directories: C:\cpp_src\vcpkg\installed\x64-windows\lib<br>
- Add to  Linker / Input /  Additional Dependencies:<br>
glad.lib<br>
opengl32.lib<br>
SDL2.lib<br>
(plus any others you need, like SDL2_image.lib if you use them).<br>
- Add to Build Events / Post Build: copy /y C:\cpp_src\vcpkg\installed\x64-windows\bin\SDL2.dll $(OutDir)
