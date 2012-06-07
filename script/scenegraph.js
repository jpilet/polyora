/*
 * Software developed by Julien Pilet, at Keio University in 2009.
 */

/*
   TODO: 
   - layers
*/

// tell supplementary not to draw anything on detected surfaces
Qt.window.drawTransformed = false;

function make_array(a) {
	if (a instanceof Array) return a;
	if (typeof a == "undefined") return [];
	return [a];
}

// Evaluates a function or an immediate value.
function evalfi(arg, timebase) {
	if (arg instanceof Function) return arg(timebase);
	return arg;
}

function update_all_children(children, timebase) {
	//print('update_all_children: ' + children.length + ' t=' + timebase);
	if (children.length == 0) return true;
	var r = false;
	for (var i in children)
		if (children[i].update(timebase)) r=true;
	return r;
}

function apply_new(t) {
	return function() {
		var r = new t();
		t.apply(r, arguments);
		return r;
	}
}

//
// Class Node
//
function CNode(c) {
	//print('node with children:' + c);
	this.children = make_array(c);
}
this.Node = apply_new(CNode);
CNode.prototype.className = "Node";
CNode.prototype.update = function(timebase) { 
	//print('CNode.update('+this.children+')'); 
	return update_all_children(this.children, timebase); 
}
CNode.prototype.draw = function() {
	//print('CNode.draw(' + this.children + ')');
	//if (typeof this.children != "undefined" && this.children.length>0)
	for (var i in this.children) {
		//print('Draw ' + typeof this.children + ': ' + i + ' ' + this.children[i])
		this.children[i].draw();
	}
}
function _indent(n) {
	var r ='';
	for (var i=0; i<n; i++)
		r = r+' ';
	return r;
}
CNode.prototype.print = function(level) {
	print(_indent(level) + this.className + ':');

	if (typeof(this.hidden_children) != "undefined")
		for (var c in this.hidden_children)
			this.hidden_children[c].print(level+1);
	for (var c in this.children)
		this.children[c].print(level+1);
	//print(_indent(level) + '}');
}

//
// Class AllValid
//
function CAllValid(children) {
	this.hidden_children= make_array(children);
	this.children=[];
}
this.AllValid = apply_new(CAllValid);

CAllValid.prototype = new CNode();
CAllValid.prototype.className = "AllValid";
CAllValid.prototype.update = function(timebase) {
	var valid = true;
	for (var i in this.hidden_children) {
		if (!this.hidden_children[i].update(timebase)) {
			valid=false;
			break;
		}
	}
	this.children = (valid ? this.hidden_children : []);
	return valid;
}

//
// Class StopAfter
//
function CStopAfter(duration, children) {
	this.hidden_children= make_array(children);
	this.children=[];
	this.duration=duration;
}
this.StopAfter = apply_new(CStopAfter);

CStopAfter.prototype = new CNode();
CStopAfter.prototype.className = "StopAfter";
CStopAfter.prototype.update = function(timebase) {
	//print('CStopAfter.update()');
	if (timebase < evalfi(this.duration, timebase)) {
		this.children = this.hidden_children;
		return update_all_children(this.children, timebase);
	} else {
		this.children = [];
		return false;
	}
}

//
// Class Delay
//
function CDelay(time, children) {
	this.hidden_children = make_array(children);
	this.children = [];
	this.delay_time = time;
}
this.Delay = apply_new(CDelay);
CDelay.prototype = new CNode();
CDelay.prototype.className = "Delay";
CDelay.prototype.update = function(timebase) {
	var delay = evalfi(this.delay_time, timebase);
	if (timebase < delay) {
		if (this.children.length>0) 
			this.children = [];
		return true;
	}
	this.children = this.hidden_children;
	return update_all_children(this.children, timebase - delay);
}

//
// Class Condition
//
function CCondition(cond, ifchildren, elsechildren) {
	this.condition_children= make_array(cond);
	this.ifchildren= make_array(ifchildren);
	this.elsechildren = make_array(elsechildren);
	this.children = [];
}
this.Condition = apply_new(CCondition);

CCondition.prototype = new CNode();
CCondition.prototype.className = "Condition";
CCondition.prototype.update = function(timebase) {
	if (update_all_children(this.condition_children, timebase)) {
		if (this.children != this.ifchildren) {
			this.children = this.ifchildren;
			this.delay= timebase;
		}
	} else {
		if (this.children != this.elsechildren) {
			this.children = this.elsechildren;
			this.delay = timebase;
		}
	}

	if (this.children.length>0)
		return update_all_children(this.children, timebase-this.delay);
	else
		return false;
}

//
// Class Trigger
//
function CTrigger(test, ifchildren, elsechildren) {
	this.test = test;
	this.ifchildren= make_array(ifchildren);
	this.elsechildren = make_array(elsechildren);
	this.children = this.elsechildren;
	this.delay=0;
}
CTrigger.prototype = new CNode();
CTrigger.prototype.className = "Trigger";
this.Trigger = apply_new(CTrigger);

CTrigger.prototype.update = function(timebase) {

	if (this.children == this.ifchildren) {
		// ongoing
		if (!update_all_children(this.children, timebase-this.delay)) {
			this.delay = timebase;
			if (!this.test(timebase-this.delay))
				this.children = this.elsechildren;
			return update_all_children(this.children, timebase-this.delay);
		}
	} else {
		if (this.test(timebase-this.delay)) {
			this.delay = timebase;
			this.children = this.ifchildren;
		}
		return update_all_children(this.children, timebase-this.delay);
	}
}


//
// Class RandomSelect
//
function CRandomSelect(children) {
	this.hidden_children = children;
	this.children = [];
	this.delay = 0;
	this.once = false;
}
this.RandomSelect = apply_new(CRandomSelect);
this.RandomSelectOnce = function(c) {
	var r = new CRandomSelect(c);
	r.once = true;
	return r;
}
CRandomSelect.prototype = new CNode();
CRandomSelect.prototype.className = "RandomSelect";
CRandomSelect.prototype.update = function(timebase) {

	if (this.children.length > 0) {
		if (this.children[0].update(timebase - this.delay)) 
			return true;
		else if (this.once) {
			this.children = [];
			return false;
		}
	}

	// saves delay
	this.delay = timebase;

	// randomly choose a valid child
	var valid = [];
	for (var i in this.hidden_children) {
		if (this.hidden_children[i].update(timebase - this.delay)) 
			valid.push(this.hidden_children[i]);
	}
	if (valid.length == 0) {
		this.children = [];
		return false;
	}

	var n = Math.floor(Math.random() * valid.length);
	print('Selecting entry ' + n + ' out of ' + valid.length);
	this.children = [ valid[n] ];
	return true;
}

//
// Class Sequence
//
function CSequence(c) {
	this.hidden_children = make_array(c);
	this.children = [];
	this.selected = -1;
	this.delay=0;
	this.firstValid = false;
}
this.Sequence = apply_new(CSequence);
this.FirstValid = function() {
	var r = new CSequence();
	CSequence.apply(r, arguments);
	r.firstValid = true;
	return r;
}

CSequence.prototype = new CNode();
CSequence.prototype.className = "Sequence";
CSequence.prototype.update = function(timebase) {


	if (this.hidden_children.length <1) 
		return false;
	if (this.selected<0) {
		this.selected=0;
		this.delay = timebase;
		this.children = [ this.hidden_children[this.selected] ];
	} else if (this.firstValid) {
		if (this.children[0].update(timebase - this.delay))
			return true;
		this.selected=0;
		this.delay = timebase;
		this.children = [ this.hidden_children[this.selected] ];
	}

	var n = this.hidden_children.length;
	while (!this.children[0].update(timebase - this.delay) ) {
		if (n-- == 0) {
			this.selected = -1;
			this.children = [];
			return false;
		}

		this.selected = this.selected + 1;
		if (this.selected >= this.hidden_children.length) this.selected=0;
		this.delay = timebase;
		this.children = [ this.hidden_children[this.selected] ];
	}
	return true;
}


//
// Class FirstValidInterrupt
//
function CFirstValidInterrupt(c) {
	this.hidden_children = make_array(c);
	this.children = [];
	this.selected = -1;
	this.delay=0;
}
this.FirstValidInterrupt = apply_new(CFirstValidInterrupt);

CFirstValidInterrupt.prototype = new CNode();
CFirstValidInterrupt.prototype.className = "FirstValidInterrupt";
CFirstValidInterrupt.prototype.update = function(timebase) {

	if (this.hidden_children.length <1) 
		return false;

	for (var i in this.hidden_children) {
		var delay = timebase;
		if (this.hidden_children[i] == this.children[0]) 
			delay = this.delay;
		if (this.hidden_children[i].update(timebase - delay)) {
			if (this.children[0] != this.hidden_children[i]) {
				this.delay = timebase;
				this.children = [this.hidden_children[i]];
			}
			return true;
		}
	}
	return false;
}

//
// Class Color
//
function CColor(r,g,b,a, c) {
	this.r = r;
	this.g = g;
	this.b = b;
	this.a = a;
	this.children = make_array(c);
}
this.Color = apply_new(CColor);
CColor.prototype = new CNode();
CColor.prototype.className = "Color";
CColor.prototype.update = function(timebase) {
	this._r = evalfi(this.r, timebase);
	this._g = evalfi(this.g, timebase);
	this._b = evalfi(this.b, timebase);
	this._a = evalfi(this.a, timebase);
	return update_all_children(this.children, timebase);
}
CColor.prototype.draw = function() {
	//print('Color is now: ' + [this._r, this._g, this._b, this._a]);
	graphics.pushColor(this._r, this._g, this._b, this._a);
	CNode.prototype.draw.apply(this);
	graphics.popColor();
}

//
// Class LineWidth
//
function CLineWidth(width, c) {
	this.width = width;
	this.children = make_array(c);
}
this.LineWidth = apply_new(CLineWidth);
CLineWidth.prototype = new CNode();
CLineWidth.prototype.className = "LineWidth";
CLineWidth.prototype.update = function(timebase) {
	this._width = evalfi(this.width, timebase);
	return update_all_children(this.children, timebase);
}
CLineWidth.prototype.draw = function() {
	graphics.pushLineWidth(this._width);
	CNode.prototype.draw.apply(this);
	graphics.popLineWidth();
}

//
// Class Quad
//
function CQuad() {
	this.points = Array.prototype.slice.apply(arguments, [0]);
	this._points = [0,0,0,0,0,0,0,0];
}
this.Quad = apply_new(CQuad);

CQuad.prototype = new CNode();
CQuad.prototype.className = "Quad";
CQuad.prototype.draw = function() {
	graphics.drawQuad(this._points[0],
			this._points[1],
			this._points[2],
			this._points[3],
			this._points[4],
			this._points[5],
			this._points[6],
			this._points[7]);
}
CQuad.prototype.update = function(timebase) { 

	if (this.points.length == 4) {
		for (var i=0; i<4; i++) {
			var p = evalfi(this.points[i], timebase);
			this._points[i*2] = p.x;
			this._points[i*2+1] = p.y;
		}
	} else if (this.points.length == 8) {
		for (var i=0; i<8; i++) {
			this._points[i] = evalfi(this.points[i], timebase);
		}
	} else {
		throw "Quad requires 4 or 8 arguments representing 4 points";
	}
	return true; 
}

//
// Class LineStrip
//
function CLineStrip(x_array, y_array) {
	this.x_array = x_array;
	this.y_array = y_array;
	this._x_array = [];
    if (y_array != undefined)
        this._y_array = [];
}
this.LineStrip = apply_new(CLineStrip);

CLineStrip.prototype = new CNode();
CLineStrip.prototype.className = "LineStrip";
CLineStrip.prototype.draw = function() {
    if (this._y_array != undefined) {
        graphics.drawLineStrip(this._x_array, this._y_array);
    } else {
        graphics.drawLineStrip(this._x_array);
    }
}
CLineStrip.prototype.update = function(timebase) { 
    this._x_array = evalfi(this.x_array, timebase);
    if (this._y_array != undefined) {
        this._y_array = evalfi(this.y_array, timebase);
    }
	return true; 
}

//
// Class Rect
//
function CRect(x,y,w,h) {
	this.ax=x;
	this.ay=y;
	this.aw=w;
	this.ah=h;
	this.update(0);
}
this.Rect = apply_new(CRect);

CRect.prototype = new CNode();
CRect.prototype.className = "Rect";
CRect.prototype.draw = function() {
	graphics.drawRectangle(this);
}
CRect.prototype.update = function(timebase) { 
	this.x = evalfi(this.ax, timebase);
	this.y = evalfi(this.ay, timebase);
	this.w = evalfi(this.aw, timebase);
	this.h = evalfi(this.ah, timebase);
	return true;
}

//
// Class Surface
//
function CSurface(model, c) {
	this.model=model;
	this.hidden_children = make_array(c);
	this.delay=0;
	this.activated = false;
}
this.Surface = apply_new(CSurface);

CSurface.prototype = new CNode();
CSurface.prototype.className = "Surface";
CSurface.prototype.draw = function() {
	this.model.texture = graphics.texture;
	this.model.display();
	CNode.prototype.draw.apply(this);
}
CSurface.prototype.update = function(timebase) { 
	if (!this.model.detected) {
		if (this.activated) {
			this.children = [];
			this.activated = false;
		}
		return false;
	}
	if (!this.activated) {
		this.activated = true;
		this.children = this.hidden_children;
		this.delay = timebase;
	}
	return update_all_children(this.children, timebase - this.delay);
}

//
// Class Image
//
function CImage(name, c) {
	if (arguments.length == 0) return;
	this.texture = graphics.createTexture();
	this.name = name;

	if (!this.texture.loadDDS(this.name)) {
		throw("Can't load DDS: '" + this.name + "'");
	}
	this.children = make_array(c);
}
this.Image = apply_new(CImage);

CImage.prototype = new CNode();
CImage.prototype.className = "Image";
CImage.prototype.draw = function() {
	//if (!this.texture.loadDDS(this.name)) { throw("Can't load DDS: '" + this.name + "'"); }
	var t = graphics.texture;
	graphics.texture = this.texture;
	CNode.prototype.draw.apply(this);
	graphics.texture = t;
}

//
// Class Video
//
function CVideo(params, c) {
	if (arguments.length==0) return;
	this.hidden_children = make_array(c);
	this.fps=15;
	this.name = '';
	this.params = params;
	this.loop = false;
	this.start=-1;
	this.stop=-1;
	this.texture = graphics.createTexture();
	this.loopFromFrame=0;
	this.extendLastFrame = false;
	if (params instanceof Object) {
		if ("name" in params) this.name = params.name;
		else {
			print("Video: name property is missing. Please use: {name: 'filename'}")
		}
		if ("fps" in params) this.fps=params.fps;
		if ("loop" in params) this.loop = params.loop;
		if ("start" in params) this.start = params.start;
		if ("stop" in params) this.stop = params.stop;
		if ("loopFrom" in params) this.loopFromFrame = params.loopFrom;
		if ("extendLastFrame" in params) this.extendLastFrame = params.extendLastFrame;
	} else {
		this.name = params.toString();
	}
	if (!this.texture.openVideo(this.name, this.fps, this.loop, this.start, this.stop)) {
		print("Can't load video: '" + this.name + "'");
		throw "FileNotFound";
	}	
	this.texture.loopFromFrame = this.loopFromFrame;
	this.duration = (this.texture.lastFrame - this.texture.firstFrame) / this.fps;
}
this.Video = apply_new(CVideo);

CVideo.prototype = new CImage();
CVideo.prototype.className = "Video";
CVideo.prototype.update = function(timebase) {
	this.texture.videoTime=timebase;
	if (this.extendLastFrame || this.loop || timebase < this.duration) {
		this.children = this.hidden_children;
	} else {
		this.children = [];
		return false;
	}
	return update_all_children(this.children, timebase);
}

//
// Translate Class
//
function CTranslate() {
	if (arguments.length==0) return;
	if (arguments.length != 3 && arguments.length !=2)
		throw("Translate requires either 2 or 3 arguments.");
	if (arguments.length == 2)
		this.vector = arguments[0];
	else
		this.vector = [arguments[0], arguments[1] ];
	this.children = make_array(arguments[arguments.length-1]);
	this._x = 0;
	this._y = 0;
}
this.Translate = apply_new(CTranslate);
CTranslate.prototype = new CNode();
CTranslate.prototype.className = "Translate";
CTranslate.prototype.update = function(timebase) {
	if (this.vector instanceof Array) {
		this._x = evalfi(this.vector[0], timebase);
		this._y = evalfi(this.vector[1], timebase);
	} else {
		var p = evalfi(this.vector, timebase);
		this._x = p.x;
		this._y = p.y;
	}
	return update_all_children(this.children, timebase);
}
CTranslate.prototype.draw = function() {
	graphics.pushMatrix();
	graphics.translate(this._x, this._y);
	CNode.prototype.draw.apply(this);
	graphics.popMatrix();
}

//
// Scale Class
//
function CScale() {
	if (arguments.length==0) return;
	if (arguments.length != 3 && arguments.length !=2)
		throw("Scale requires either 2 or 3 arguments.");
	if (arguments.length == 2)
		this.vector = arguments[0];
	else
		this.vector = [arguments[0], arguments[1] ];
	this.children = make_array(arguments[arguments.length-1]);
	this._x = 0;
	this._y = 0;
}
this.Scale = apply_new(CScale);
CScale.prototype = new CNode();
CScale.prototype.className = "Scale";
CScale.prototype.update = function(timebase) {
	if (this.vector instanceof Array) {
		this._x = evalfi(this.vector[0], timebase);
		this._y = evalfi(this.vector[1], timebase);
	} else {
		var p = evalfi(this.vector, timebase);
		this._x = p.x;
		this._y = p.y;
	}
	return update_all_children(this.children, timebase);
}
CScale.prototype.draw = function() {
	graphics.pushMatrix();
	graphics.scale(this._x, this._y);
	CNode.prototype.draw.apply(this);
	graphics.popMatrix();
}

//
// Rotate Class
//
function CRotate(angle, pivot, c) {
	if (arguments.length==0) return;

	if (arguments.length==3)
		this.vector = arguments[1];
	else 
		this.vector = [arguments[1], arguments[2]];

	this.children = make_array(arguments[arguments.length-1]);
	this.angle = angle;
	this._angle = 0;
	this._x=0;
	this._y=0;
}
this.Rotate = apply_new(CRotate);
CRotate.prototype = new CNode();
CRotate.prototype.className = "Rotate";
CRotate.prototype.update = function(timebase) {
	this._angle = evalfi(this.angle, timebase);
	if (this.vector instanceof Array) {
		this._x = evalfi(this.vector[0], timebase);
		this._y = evalfi(this.vector[1], timebase);
	} else {
		var p = evalfi(this.vector, timebase);
		this._x = p.x;
		this._y = p.y;
	}
	return update_all_children(this.children, timebase);
}
CRotate.prototype.draw = function() {
	//print('Rotate: ' + [this._angle, this._x, this._y]);
	graphics.pushMatrix();
	graphics.rotate(this._angle, this._x, this._y);
	CNode.prototype.draw.apply(this);
	graphics.popMatrix();
}

//
// Homography Class
//
function CHomography(pts, children) {
	if (arguments.length==0) return;
	this.children = make_array(children);
	this.points = pts;
	if (! (pts instanceof Array) || pts.length!=4) {
		print("First parameter for Homography must be an array of 4 attached points.");
		throw "TypeError";
	}
	this._points = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ];
}
this.Homography = apply_new(CHomography);
CHomography.prototype = new CNode();
CHomography.prototype.className = "Homography";
CHomography.prototype.update = function(timebase) {
	for (var i=0; i<4; ++i) {
		var p = evalfi(this.points[i], timebase);
		var base=2*i;
		this._points[base] = p.src.x;
		this._points[base + 1] = p.src.y;
		base += 8;
		this._points[base] = p.x;
		this._points[base + 1] = p.y;
	}
	return update_all_children(this.children, timebase);
}
CHomography.prototype.draw = function() {
	// it also pushes the matrix
	graphics.homography(this._points); 
	CNode.prototype.draw.apply(this);
	graphics.popMatrix();
}

//
// Polyora Class
//
function CPolyora(target_id, children) {
	if (arguments.length==0) return;
	this.children = make_array(children);
	this.target = polyora.getTarget(target_id);
	if (this.target == undefined) {
	    throw("Polyora object " + target_id + " not found.");
	}
}
this.Polyora = apply_new(CPolyora);
CPolyora.prototype = new CNode();
CPolyora.prototype.className = "Polyora";
CPolyora.prototype.update = function(timebase) {
    if (this.target.lost) {
	return false;
    } else {
	return update_all_children(this.children, timebase);
    }
}
CPolyora.prototype.draw = function() {
    if (this.target.lost) {
	return false;
    }
    this.target.pushTransform();
    CNode.prototype.draw.apply(this);
    this.target.popTransform();
}

//
// PolyoraRelTime Class
//
function CPolyoraRelTime(target_id, children) {
	if (arguments.length==0) return;
	this.children = make_array(children);
	this.target = polyora.getTarget(target_id);
	if (this.target == undefined) {
	    throw("Polyora object " + target_id + " not found.");
	}
}
this.PolyoraRelTime = apply_new(CPolyoraRelTime);
CPolyoraRelTime.prototype = new CPolyora();
CPolyoraRelTime.prototype.className = "PolyoraRelTime";
CPolyoraRelTime.prototype.update = function(timebase) {
    if (this.target.lost) {
	return false;
    } else {
	return update_all_children(this.children, this.target.sinceLastAppeared);
    }
}

//
// Particles Class
//
function CParticles(params, c) {
	this.params = params;
	this.particles = [];
	this.hidden_children = c;
	this.lastCreated = 0;
}
this.Particles = apply_new(CParticles);
CParticles.prototype = new CNode();
CParticles.prototype.className = "Particles";

CParticles.prototype.update = function(time) {

	var kept_part = [];
	this.particles = this.particles.concat(this.createParticles(time));
	for (var i in this.particles) {
		var p = this.particles[i];
		if (p.update(time-p.birthTime))
			kept_part.push(p);
	}
	this.particles = kept_part;
	return this.particles.length>0;
}

CParticles.prototype.draw = function() {
	for (var i in this.particles) {
		var p = this.particles[i];
		p.draw();
	}
}


//
// State machine class.
//
this.StateMachine = function() {
    this.states = { };
    this.children = [];
    this.current_state = "";
    this.delay = 0;
    this.current_time = 0;
}
StateMachine.prototype = new CNode();
StateMachine.prototype.className = "StateMachine";
StateMachine.prototype.addState = function(state_name, children, transition) {
    this.states[state_name] = {
	"children" : make_array(children),
	"transition" : transition
    };
}

StateMachine.prototype.setState = function(new_state) {
    if (new_state == this.current_state)
	return;

    this.current_state = new_state;
    this.children = this.states[new_state].children;
    this.transition = this.states[new_state].transition;
    this.delay = this.current_time;
    this.children_active = update_all_children(this.children, 0); 
}

StateMachine.prototype.update = function(timebase) {
    this.current_time = timebase;
    this.children_active = update_all_children(this.children, timebase - this.delay); 
    this.transition(timebase - this.delay, this, this.children_active);
    return this.children_active;
}
