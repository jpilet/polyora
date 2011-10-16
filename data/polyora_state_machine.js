/*
In this example, we show how to let the user control a square.
The square can be either: in the center, up, down, left, or right.
Moving a target in one direction moves the square. The square stays there for 1
second, and then returns to the center.

This is implemented with a state machine.
*/

// The thing the state machine will move.
var quad = Translate(320 - 50, 200 - 50, Quad(0,0, 0,100, 100, 100, 100,0));

// The target that will be used to control the state machine.
var motion_target = polyora.getTarget(49);

// The state machine itself
var machine = new StateMachine();

// We add a new state.
machine.addState(
	"center",  // it has a name.
	quad,  // it has a child, or an array of children

	// and it has a function computing transitions. The function is passed
	// the following parameters:
	//   't' is the time in seconds since the state has been activated.
	//   'machine' is the state machine.
	//   'children_active' is a boolean. It tells if the children are still active.
	function(t, machine, finished) {
		// We read the speed of the target.
		var speed = newPoint(0, 0);
		motion_target.getSpeed(320, 200, speed);
		// If the speed is large enough, we switch state.
		if (speed.x > 150) machine.setState("right");
		else if (speed.x < -150) machine.setState("left");
		else if (speed.y > 150) machine.setState("down");
		else if (speed.y < -150) machine.setState("up");
	});

// Create an animation that stops after a few frames.
var anim = Video({name: "papillon_def/frame%03d.dds", fps: 20, loop:false }, quad);

// The states are not very fancy. Once the anim reaches its end, we move back
// to the state 'center'.
machine.addState(
	"left",
	Translate(-150, 0, anim),
	function(t, machine, children_active) {
		if (!children_active) machine.setState("center");
	});

machine.addState(
	"right",
	Translate(150, 0, anim),
	function(t, machine, children_active) {
		if (!children_active) machine.setState("center");
	});
machine.addState(
	"down",
	Translate(0, 100, anim),
	function(t, machine, children_active) {
		if (!children_active) machine.setState("center");
	});
machine.addState(
	"up",
	Translate(0, -100, anim),
	function(t, machine, children_active) {
		if (!children_active) machine.setState("center");
	});

// Set the initial state.
machine.setState("center");

// The state machine is a normal element of the scene graph.
this.state_machine_test = Color(0, 0, 1, .9, machine);
