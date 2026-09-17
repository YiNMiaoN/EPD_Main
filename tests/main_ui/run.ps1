$ErrorActionPreference = 'Stop'
$testProject = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $testProject
try {
    $testIncludes = @('-Itests/esp_com/stubs', '-ICore/Inc', '-IFIFO/Inc', '-IApiRefresh/Inc', '-IApiRefresh/ThirdParty/coreJSON')
    $testSources = @{
        'core_json' = 'ApiRefresh/ThirdParty/coreJSON/core_json.c'
        'local_clock' = 'Core/Src/LocalClock.c'
        'heartbeat' = 'Core/Src/SystemHeartbeat.c'
    }
    foreach ($name in $testSources.Keys) {
        & gcc -std=c11 -Wall -Wextra -Werror @testIncludes -c $testSources[$name] -o "cmake-build-debug/test_$name.o"
        if ($LASTEXITCODE -ne 0) { throw "Failed to compile $name" }
    }
    & gcc -std=c11 -Wall -Wextra -Werror @testIncludes tests/main_ui/test_clock.c cmake-build-debug/test_local_clock.o -o cmake-build-debug/test_clock.exe
    if ($LASTEXITCODE -ne 0) { throw 'Failed to compile clock tests' }
    & ./cmake-build-debug/test_clock.exe
    if ($LASTEXITCODE -ne 0) { throw 'Clock tests failed' }
    foreach ($suite in @('clock_ui', 'weather', 'hitokoto')) {
        & g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -include tests/main_ui/stubs/display.h @testIncludes -IDisp/Inc -IQWeather/Inc "tests/main_ui/test_$suite.cpp" Disp/Src/HitokotoText.cpp Disp/Src/JsonText.cpp Disp/Src/WeatherData.cpp Disp/Src/MainUI.cpp Core/Src/Epd_Api.cpp cmake-build-debug/test_core_json.o cmake-build-debug/test_local_clock.o cmake-build-debug/test_heartbeat.o -o "cmake-build-debug/test_$suite.exe"
        if ($LASTEXITCODE -ne 0) { throw "Failed to compile $suite tests" }
        & "./cmake-build-debug/test_$suite.exe"
        if ($LASTEXITCODE -ne 0) { throw "$suite tests failed" }
    }
} finally {
    Pop-Location
}
