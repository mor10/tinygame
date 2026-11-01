// Emscripten 4's loop control is a C API. Keep the familiar Module API for hosts.
Module.pauseMainLoop = () => Module._tg_sim_pause();
Module.resumeMainLoop = () => Module._tg_sim_resume();
