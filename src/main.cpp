/*----------------------------------------------------------------------------*/
/*                                                                            */
/*    Module:       main.cpp                                                  */
/*    Author:       James                                                     */
/*    Created:      Thu Mar 21 2019                                           */
/*    Description:  V5 project                                                */
/*                                                                            */
/*----------------------------------------------------------------------------*/
#include "vex.h"

#include "gifclass.h"

using namespace vex;

int main() {
  vex::Gif gif("world.gif", 120, 0 );

  while(1) {
    Brain.Screen.printAt( 5, 230, "frame %3d", gif.getFrameIndex() );
    this_thread::sleep_for(10);
  }
}