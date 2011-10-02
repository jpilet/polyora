include('scenegraph.js');

explosion = new Qt.Sound("Explosion.wav");


//
// test timing and animation
//
var q = Quad(0,0, 0,100, 100, 100, 100,0);

var rotating = Rotate(function(t){ return t*30;}, 50, 50, q);

var test_timing = Node([ 
	Color(1,0,0,function(t){return Math.sin(t)/2+.5;}, q),
	Translate(200,200, rotating),
	Translate(0,200, Sequence([
		StopAfter(2,  q),
		StopAfter(5, Color(0,1,0,1, q)),
		StopAfter(3, Color(0,0,1,1,q))
	]))
]);

//
// apply an image to everything:
//
var test_image = Image("swissair_happy_512_512.dds", test_timing);

//
// apply a video to everything:
//
var test_video = Video({name: "papillon_def/frame%03d.dds", fps: 20, loop:true }, test_timing);

//
// several ways of using videos:
//
var test_video2 = Node([ 
	Video("papillon_def/frame%03d.dds", q), // use default parameters
	Video({name: "papillon_def/frame%03d.dds", fps: 40, loop:false, extendLastFrame:false }, Translate(120,0,q)),
	Video({name: "papillon_def/frame%03d.dds", fps: 40, loop:false, extendLastFrame:true }, Translate(120,120,q)),
	Video({name: "papillon_def/frame%03d.dds", loop:true }, Translate(240,0,q)),
	Video({name: "papillon_def/frame%03d.dds", fps: 40, loop:true, loopFrom:300, stop:320 }, Translate(360,0,q))
	]);


var test_scene_select = FirstValid([
	RandomSelectOnce([
		StopAfter(2,q),
		StopAfter(2,Color(1,0,0,1,q))
	])
]);

//
// Particle example
//
function BounceParticle() {
	this.pos = newPoint(0,0);
	this.speed = newPoint(256,256);
	this.max = newPoint(512,384);
	this.min = newPoint(0,0);
	this.lastUpdate=0;
	this.update = function(time) {
		var delta = time-this.lastUpdate;
		this.lastUpdate = time;

		// move the particle
		this.pos.add(this.speed.times(delta));

		// make it bounce on the 4 borders
		if (this.pos.x < this.min.x) {
			this.pos.x = 2*this.min.x-this.pos.x;
			this.speed.x = -this.speed.x;
		}
		if (this.pos.y <0) {
			this.pos.y = 2*this.min.y-this.pos.y;
			this.speed.y = -this.speed.y;
		}
		if (this.pos.x >this.max.x) {
			this.pos.x = 2*this.max.x - this.pos.x;
			this.speed.x = -this.speed.x;
		}
		if (this.pos.y >this.max.y) {
			this.pos.y = 2*this.max.y - this.pos.y;
			this.speed.y = -this.speed.y;
		}
		return this.pos;
	}
}

var particle = new BounceParticle();
var test_particle = Translate(function(t){ return particle.update(t); }, q);

var test_trigger = 
	Trigger(function(t) { return t>3; }, StopAfter(2,Color(1,0,0,1, q)), Color(0,0,0,1, q));

var test_all = Sequence([
	StopAfter(10, test_timing),
	StopAfter(10, test_image),
	StopAfter(10, test_video),
	StopAfter(10, test_video2),
	StopAfter(10, test_scene_select),
	StopAfter(10, test_particle),
       	]);

// "Polyora" changes the geometry, so that all relative children are drawn
// relative to the reference image coordinates instead of screen coordinates.
var zurich = Polyora(46, test_all);
zurich.target.appeared.connect(function() { explosion.play(); });

// With 'PolyoraRelTime', the time for children is relative to when the object appeared.
// With 'Polyora', the time for children is not modified.
// For example, with the following line, test_video2 will start from the
// beginning every time the object appears.
var hawking = PolyoraRelTime(47, test_video2);

var test_polyora = Node([zurich, test_particle, hawking, Color(.2,.5,.6, .8, q) ]);

// which test ?
var world = test_polyora

// Print a summary of our world
world.print(0);

// keep time
var time = new Qt.Time();
time.start();

// let's go
graphics.onPaint.connect(function() {
	world.update(time.elapsed());
	world.draw();
});

