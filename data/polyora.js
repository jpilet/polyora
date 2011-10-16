include('scenegraph.js');

include('polyora_state_machine.js');

var world = state_machine_test;

// keep time
var time = new Qt.Time();
time.start();

// let's go
graphics.onPaint.connect(function() {
	world.update(time.elapsed());
	world.draw();
});

