# Timewarp: OpenGL AI Shaders and other Experiments

Right mouse click project and open nuget package manager and install vcpkg<br>
or use git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg<br><br>
Inside Developer Powershell:<br>
- cd C:\dev\vcpkg
- vcpkg install sdl2<br>
- vcpkg install sdl3<br>
- vcpkg install glad:x64-windows<br>
- vcpkg install glew:x64-windows<br>
- vcpkg integrate install<br>

Right mouse click project / properties:<br>
- Add to C++ / General / Additional Include Directories: C:\dev\vcpkg\installed\x64-windows\include<br>
If still necessary:
- Add to Linker / General / Additional library Directories: C:\dev\vcpkg\installed\x64-windows\lib<br>
- Add to  Linker / Input /  Additional Dependencies:<br>
glad.lib<br>
opengl32.lib<br>
SDL2.lib<br>
(plus any others you need, like SDL2_image.lib if you use them).<br>
- Add to Build Events / Post Build: copy /y C:\dev\vcpkg\installed\x64-windows\bin\SDL2.dll $(OutDir)

<img src=https://github.com/RayColt/timewarp/blob/master/.gitfiles/timewarp.jpg />
<img src=https://github.com/RayColt/timewarp/blob/master/.gitfiles/timewarp2.jpg />
