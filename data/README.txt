
		 * Polyora: How to add and augment an object. *

1) Start polyora

2) Press "3"

3) Show the target object to the camera

4) When the target object occupies a reasonable area on the screen, hit 'h'
   Green and red shapes should appear and stick on the target.

5) Slowly move the object around. If the tracking is lost (no more green stuff),
   simply move the target close to where it was when 'h' has been pressed. When
   tracking starts again, slowly move the object away again.

6) When polyora detects and tracks the object as wanted, hit space to pause
   polyora and open objectXXX.png that was created by pressing 'h'.

7) Design the augmented layer using photoshop or any other software. Write down
   the coordinates of the augmented layer, in image coordinates.
   Save your layer in DDS format.

8) Edit polyora.js using your favorite text editor. Add a line of the form:
   var world =
       Polyora(XXX, Image("layer.dds", Quad(0,0, 640,0, 640,480, 0, 480));
   where XXX is the object number in <objectXXX.png>.  The parameters of Quad()
   define where to draw the layer, in coordinates of objectXXX.png. Unit:
   pixels. Origin: top left (0,0). Bottom right: (width, height). 

9) In polyora, press space to resume (if it was paused), '5' to switch to
   scripted augmentation, and 'l' (small L) to reload polyora.js. Polyora should
   now augment the target.


				    Example

Let's assume that you created object050.png with polyora. The tracking works
fine, because the object is textured enough, like a photo.
object050.png is a 640x480 image, it shows a cartoon character. You want to
augment it with a blinking red nose.

You open object050.png in photoshop, create a new transparent layer, and draw
the nose. You can now shrink the nose layer to get rid of the useless
transparent pixels.
Now, you write down the coordinates of the 4 corners of the layer on
object050.png, starting with the top-left corner, and taking them in a clockwise
order. You find: 
    top left: (187, 124),
    top right: (265, 124),
    bottom right: (265, 203), and
    bottom left: (187, 203).
You can already edit polyora.js and write:
var nose_quad = Quad(187, 124, 265, 124, 265, 203, 187, 203);

Now, let's come back to the nose layer. You can create a new image that contains
only this layer. You can resample the layer if you want to change its
resolution (to 64x64 or 128x128, for example). Now save it to red_nose.dds. Make
sure you select a RGBA format that includes transparency.

In polyora.js, you can now write:

var red_nose = Image("red_nose.dds", nose_quad);
var world = Polyora(50, red_nose);

Now, Polyora should display the cartoon character with a red nose. Let's make it
blink. We can use the Color() node with dynamic values to achieve our goal.

var world = Polyora(50, Color(
	    1,1,1,  // R,G,B = 1: white. Will not modify the color of red_nose.dds.
	    function(time) {  // alpha value. We pass a function instead of a constant.
	       // The sinus function provides the oscillation we need.
	       // sinus returns a number between -1 and 1. Dividing it by two
	       // and adding 0.5 changes the range to [0, 1].
	       // 0 is completely transparent,
	       // 1 is completely opaque.
	       //
	       return Math.sin(time * 60) / 2 + 0.5;
	    },
	    red_nose));

Now, we should obtain a blinking nose. Because this is a bit cumbersome to read
and write, we extract the blinking logic to a function:

function Blink(speed, scene) {
    return Color(1,1,1, function(time) {
	    return Math.sin(time * speed) / 2 + 0.5;
    }, scene);
}

Our blinking nose scene becomes:
var world = Polyora(50, Blink(60, red_nose));

That's it. Any problem ? Try leto@calodox.org.
