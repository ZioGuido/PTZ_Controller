# PTZ_Controller
Project for controlling PTZ camera via Arduino 

This project uses a Raspberry Pi Pico, an UART TTL to RS485 transceiver and a bunch of buttons to control PTZ cameras using the Pelco-D protocol.
I started this project because I was unhappy with the original controller we bought for controlling PTZ cameras in our TV studio. 
Those joystick controllers are made for the CCTV suirvellance cameras, they're not practical for the TV studio when recording or streaming live.

There were a series of issues:
- Too many button pushes to select a camera address (ESC, ADDR, number, ENTER)
- Too many button pushes to recall a preset (ESC, PRESET, number, ENTER)
- Joystick isn't precise
- Pan and Tilt speeds ramp up too rapidly using the joystick
- Buttons are too noisy
- LCD backlight goes off after a short while and you don't know which camera address it's on until you push a boutton

For these reasons I decided to make my owh controller without using a joystick, only big buttons and a couple of potentiometers.
Unfortunately, the camera we're using don't recognize the Zoom speed parameter from Pelco-D protocol, I should try writing a similar code but with Visca.
But for the remaining points, this controller is way better: silent buttons, just one push to select a camera (up to 4 addresses), 
just one push to recall a preset (up to 6 per camera), and you can vary Pan & Tilt speed while panning or tilting in a very smooth way.

