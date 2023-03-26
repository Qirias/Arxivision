//
//  app.cpp
//  ArXivision
//
//  Created by kiri on 26/3/23.
//

#include <stdio.h>
#include "app.h"

namespace arx {

    void App::run() {
        
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
        }
    }
}
