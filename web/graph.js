
function Scale()
{
    
}

function GraphContext(block_start, block_length, display_length, display_width)
{
    var canvas = null;

    function getBlockStart() 
    {
	return block_start;
    }
    this.getBlockStart = getBlockStart;

    function setBlockStart(s) 
    {
	block_start = s;
    }
    this.setBlockStart = setBlockStart;

    
    function getBlockWidth()
    {
	return display_width * block_length / display_length;
    }
    this.getBlockWidth = getBlockWidth;

    function getBlockLength()
    {
	return block_length;
    }
    this.getBlockLength = getBlockLength;

    function setBlockLength(l)
    {
	block_length = l;
    }
    this.setBlockLength = setBlockLength;
    
    function getDisplayLength()
    {
	return display_length;
    }
    this.getDisplayLength = getDisplayLength;

    function setDisplayLength(l)
    {
	display_length = l;
    }
    this.setDisplayLength = setDisplayLength;
    
    function getDisplayWidth()
    {
	return display_width;
    }
    this.getDisplayWidth = getDisplayWidth;

    function getCanvas() 
    {
	return canvas;
    }
    this.getCanvas = getCanvas;
    
    function setCanvas(c)
    {
	canvas = c;
    }
    this.setCanvas = setCanvas;
}

function GraphBlock(context)
{
    var y_pos = 50;
    var block_height = 20;

    function build_block(time_start, time_end)
    {
	throw "build_block not implemented, implement in subclass";
    }
    this.build_block = build_block;
    
    function setYPos(pos) {
	y_pos = pos;
    }
    this.setYPos = setYPos;
    
    function getYPos() {
	return y_pos;
    }
    this.getYPos = getYPos;
    
    function setBlockHeight(h) {
	block_height = h;
    }
    this.setBlockHeight = setBlockHeight;
    
    function getBlockHeight() {
	return block_height;
    }
    this.getBlockHeight = getBlockHeight;


    function getBlockWidth() {
	return context.getBlockWidth();
    }
    this.getBlockWidth = getBlockWidth;

    
    function getCanvas() 
    {
	return context.getCanvas();
    }
    this.getCanvas = getCanvas;

    function setContext(ctxt)
    {
	context = ctxt;
    }
    this.setContext = setContext;
}

function TextGraphBlock(context)
{
    GraphBlock.call(this,context);
    this.text_width = 50;
    function build_block(time_start, time_end, ts_values)
    {
	var canvas = this.getCanvas();
	var y_pos = this.getYPos();
	var graph_width = this.getBlockWidth();
	var block_height = this.getBlockHeight();
	
	var scale = graph_width / (time_end - time_start);
	var text_repeat = this.text_width*3;
	
	for(var i = 0; i < ts_values.length; i+= 2) {
	    var start = ts_values[i] - time_start;
	    var value = ts_values[i+1];
	    if (value == null) continue;
	    var x = start * scale;
	    if (x > graph_width) continue;
	    var end_x = graph_width + 10;
	    if (i + 2 < ts_values.length)  {
		end_x = (ts_values[i+2] - time_start) * scale;
		if (end_x > graph_width + 10) {
		    end_x = graph_width + 10;
		}
	    }
	    if (x < -text_repeat) {
		x = -text_repeat;
	    }
	    var width = end_x - x;
	    //console.log("x: "+x+" width: "+width+" end "+end_x+" graph_width "+graph_width);
	    block=document.createElementNS("http://www.w3.org/2000/svg","svg:rect");
	    block.setAttribute("x", x);
	    block.setAttribute("y", y_pos);
	    block.setAttribute("width", width);
	    block.setAttribute("height", block_height);
	    block.setAttribute("style", "fill:none;stroke:black;stroke-width:1px");
	    canvas.appendChild(block);

	    for(var tx = x;
		tx == x || tx < end_x - text_repeat + 1; tx += text_repeat) {
		var text = document.createElementNS("http://www.w3.org/2000/svg",
						    "svg:text");
		text.setAttribute("style", "font-size:"+(block_height*0.8)+"px;dominant-baseline:text-after-edge");
		text.setAttribute("x", tx+1);
		text.setAttribute("y", y_pos + block_height);
	    var content = document.createTextNode(value);
		text.appendChild(content);
		canvas.appendChild(text);
	    }
	}
    }
    this.build_block = build_block;

}

TextGraphBlock.prototype = new GraphBlock();
TextGraphBlock.prototype.constructor = TextGraphBlock;



/* 
   ms		h:m:s		yyyy-mm-dd
   10ms		h:m:s		yyyy-mm-dd
   100ms	h:m:s		yyyy-mm-dd
   s		h:m		yyyy-mm-dd
   10s		h:m		yyyy-mm-dd
   m		h		yyyy-mm-dd
   10m		h		yyyy-mm-dd
   h		dd		yyyy-mm
   8h		dd		yyyy-mm
   dd		mm		yyyy
   10dd		mm		yyyy
   mm		yyyy
   yyyy
*/

function trunc(v, t) {
    return Math.floor(v/t) *t;
}
function Format_ms()
{
    this.label = "ms";
    this.format = function(date) {
	return date.getMilliseconds();
    }
    this.first = function(date) {
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 1);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 1);
    }
    
}

function Format_10ms()
{
    this.label = "ms";
    this.format = function(date) {
	return date.getMilliseconds();
    }
    this.first = function(date) {
	date.setTime(trunc(date.getTime(),10));
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 10);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 10);
    }

}

function Format_100ms()
{
    this.label = "ms";
    this.format = function(date) {
	return date.getMilliseconds();
    }
    this.first = function(date) {
	date.setTime(trunc(date.getTime(),100));
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 100);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 100);
    }
}

function Format_s()
{
    this.label = "s";
    this.format = function(date) {
	return date.getSeconds();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 1000);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 1000);
    }
}

function Format_10s()
{
    this.label = "s";
    this.format = function(date) {
	return date.getSeconds();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(trunc(date.getSeconds(),10));
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 10000);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 10000);
    }
}

function Format_m()
{
    this.label = "m";
    this.format = function(date) {
	return date.getMinutes();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(0);
    }
    this.next = function(date) {
	date.setMinutes(date.getMinutes() + 1);
    }
    this.prev = function(date) {
	date.setMinutes(date.getMinutes() - 1);
    }
}

function Format_10m()
{
    this.label = "m";
    this.format = function(date) {
	return date.getMinutes();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(0);
	date.setMinutes(trunc(date.getMinutes(),10));
    }
    this.next = function(date) {
	date.setMinutes(date.getMinutes() + 10);
    }
    this.prev = function(date) {
	date.setMinutes(date.getMinutes() - 10);
    }
}

function Format_h()
{
    this.label = "h";
    this.format = function(date) {
	return date.getHours();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(0);
	date.setMinutes(0);
    }
    this.next = function(date) {
	date.setHours(date.getHours() + 1);
    }
    this.prev = function(date) {
	date.setHours(date.getHours() - 1);
    }
}

function Format_8h()
{
    this.label = "h";
    this.format = function(date) {
	return date.getHours();
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(0);
	date.setMinutes(0);
	date.setHours(trunc(date.getHours(), 8));
    }
    this.next = function(date) {
	date.setHours(date.getHours() + 8);
    }
    this.prev = function(date) {
	date.setHours(date.getHours() - 8);
    }
}

function Format_D()
{
    this.label = "D";
    this.format = function(date) {
	return date.getDate();
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
    }
    this.next = function(date) {
	date.setDate(date.getDate() + 1);
    }
    this.prev = function(date) {
	date.setDate(date.getDate() - 1);
    }
}

function Format_10D()
{
    this.label = "D";
    this.format = function(date) {
	return date.getDate();
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
	date.setDate(trunc(date.getDate(),10));

    }
    this.next = function(date) {
	date.setDate(date.getDate() + 10);
    }
    this.prev = function(date) {
	date.setDate(date.getDate() - 10);
    }
}

function Format_M()
{
    this.label = "M";
    this.format = function(date) {
	return date.getMonth()+1;
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
	date.setDate(0);
    }
    this.next = function(date) {
	date.setMonth(date.getMonth() + 1);
    }
    this.prev = function(date) {
	date.setMonth(date.getMonth() - 1);
    }
}

function Format_Y()
{
    this.label = "Y";
    this.format = function(date) {
	return date.getFullYear();
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
	date.setDate(0);
	date.setMonth(0);
    }
    this.next = function(date) {
	date.setFullYear(date.getFullYear() + 1);
    }
    this.prev = function(date) {
	date.setFullYear(date.getFullYear() - 1);
    }
}


function padToTwo(n) {
    return ("0"+n).slice(-2);
}

function Format_hms()
{
    this.label = "h:m:s";
    this.format = function(date) {
	return date.getHours()+":"+padToTwo(date.getMinutes())+":"+padToTwo(date.getSeconds());
    }
    this.first = function(date) {
	date.setMilliseconds(0);
    }
    this.next = function(date) {
	date.setTime(date.getTime() + 1000);
    }
    this.prev = function(date) {
	date.setTime(date.getTime() - 1000);
    }
}

function Format_hm()
{
    this.label = "h:m:s";
    this.format = function(date) {
	return date.getHours()+":"+padToTwo(date.getMinutes());
    }
    this.first = function(date) {
	date.setMilliseconds(0);
	date.setSeconds(0);
    }
    this.next = function(date) {
	date.setMinutes(date.getMinutes() + 1);
    }
    this.prev = function(date) {
	date.setMinutes(date.getMinutes() - 1);
    }
}


function Format_YMD()
{
    this.label = "YYYY-MM-DD";
    this.format = function(date) {
	return date.getFullYear()+"-"+(date.getMonth()+1)+"-"+date.getDate();
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
    }
    this.next = function(date) {
	date.setDate(date.getDate() + 1);
    }
    this.prev = function(date) {
	date.setDate(date.getDate() - 1);
    }
}

function Format_YM()
{
    this.label = "YYYY-MM";
    this.format = function(date) {
	return date.getFullYear()+"-"+(date.getMonth()+1);
    }
    this.first = function(date) {
	date.setHours(0,0,0,0);
	date.setDate(0);
    }
    this.next = function(date) {
	date.setMonth(date.getMonth() + 1);
    }
    this.prev = function(date) {
	date.setMonth(date.getMonth() - 1);
    }
}



var formats =
    [{min_span:1,
      block_classes: [Format_ms, Format_hms, Format_YMD]},
     {min_span:10,
      block_classes: [Format_10ms, Format_hms, Format_YMD]},
     {min_span:100,
      block_classes: [Format_100ms, Format_hms, Format_YMD]},
     {min_span:1000,
      block_classes: [Format_s, Format_hm, Format_YMD]},
     {min_span:10000,
      block_classes: [Format_10s, Format_hm, Format_YMD]},
     {min_span:60000,
      block_classes: [Format_m, Format_h, Format_YMD]},
     {min_span:600000,
      block_classes: [Format_10m, Format_h, Format_YMD]},
     {min_span:3600000,
      block_classes: [Format_h, Format_D, Format_YM]},
     {min_span:8*3600000,
      block_classes: [Format_8h, Format_D, Format_YM]},
     {min_span:24*3600000,
      block_classes: [Format_D, Format_M, Format_Y]},
     {min_span:10*24*3600000,
      block_classes: [Format_10D, Format_M, Format_Y]},
     {min_span:28*24*3600000,
      block_classes: [Format_M, Format_Y]},
     {min_span:365*24*3600000,
      block_classes: [Format_Y]},
    ];

    

function TimeGraphBlock(context)
{
    GraphBlock.call(this,context);
    var sub_blocks = [new TextGraphBlock(parent), 
		      new TextGraphBlock(parent),
		      new TextGraphBlock(parent)];
    var current_format = null;
    var parentSetContext = this.setContext;

    function setContext(c)
    {
	parentSetContext.call(this, c);
	for (s in sub_blocks) {
	    sub_blocks[s].setContext(c);
	}
    }
    this.setContext = setContext;

    function build_block(time_start, time_end)
    {
	var sub_height = this.getBlockHeight() / 3;
	for (s in sub_blocks) {
	    sub_blocks[s].setYPos(this.getYPos() + s* sub_height)
	    sub_blocks[s].setBlockHeight(sub_height);
	}
	
	var y_pos = this.getYPos();
	var graph_width = this.getBlockWidth();
	var block_height = this.getBlockHeight();
	
	var scale = graph_width / (time_end - time_start);
	var min_text_width = 40;
	
	var start_date = new Date(time_start);
	var end_date = new Date(time_end)
	
	var format = null;
	var block_time = (time_end - time_start) * min_text_width / this.getBlockWidth();;

	for (var f in formats) {
	    format = formats[f];
	    if (format.min_span > block_time) break;
	}
	if (format.block_objects == null) {
	    format.block_objects = [];
	    for (var b in format.block_classes) {
		format.block_objects[b]= new format.block_classes[b]();
	    }
	}
	format.block_objects[0].first(start_date);
	for (var b in format.block_objects) {
	    var date = new Date(time_start);
	    var values = [];
	    var block = format.block_objects[b];
	    block.first(date)
	    while (date.getTime() < time_end) {
		values.push(date.getTime());
		values.push(block.format(date));
		block.next(date)
	    }
	    sub_blocks[b].build_block(time_start, time_end, values);
	}
	current_format = format;

    }
    this.build_block = build_block;

    function next(row, date)
    {
	if (current_format == null) return;
	if (row >= current_format.block_objects.length) return;
	current_format.block_objects[row].next(date);	
    }
    this.next = next;

    function prev(row, date)
    {
	if (current_format == null) return;
	if (row >= current_format.block_objects.length) return;
	current_format.block_objects[row].prev(date);
    }
    this.prev = prev;
}

TimeGraphBlock.prototype = new GraphBlock();
TimeGraphBlock.prototype.constructor = TimeGraphBlock;

function TimeCanvas(parent, width, height)
{
    context = new GraphContext(0,1000, 1000, width);
    this.width = width;
    this.height = height;
    
    this.display_start = 0;
    this.canvas = null;
    this.blocks = [];

    $(parent).mousedown(mouse_down);
    
    var drag_x;
    var drag_y;
    function mouse_down(event)
    {
	$(parent).mouseup(mouse_up);
	$(parent).mousemove(mouse_move);
	drag_x = event.pageX;
	drag_y = event.pageY;
	event.preventDefault();
    }
    
    function mouse_up(event)
    {
	$(parent).off("mouseup mousemove"); 
	var delta_x = event.pageX-drag_x;
	var delta_time =
	    -context.getDisplayLength() * delta_x / context.getDisplayWidth();
	self.moveTo(self.display_start+delta_time, 0);
	drag_x = event.pageX;
	drag_y = event.pageY;
	event.preventDefault();
    }

    var self = this;
    function mouse_move(event)
    {
	event.preventDefault();
    }

    function setDisplayLength(l)
    {
	context.setDisplayLength(l);
    }
    this.setDisplayLength = setDisplayLength;

    function setBlockLength(l)
    {
	context.setBlockLength(l);
    }
    this.setBlockLength = setBlockLength;
    
    function addBlock(block)
    {
	block.setContext(context);
	this.blocks.push(block);
    }
    this.addBlock = addBlock;

    function zoomTo(display_length)
    {
	var mid_display = this.display_start + context.getDisplayLength() / 2;
	this.display_start = mid_display - display_length / 2;
	context.setDisplayLength(display_length);
	var block_length = display_length * 3;
	context.setBlockLength(block_length);
	var block_start = mid_display - block_length / 2;
	context.setBlockStart(block_start);
	this.update();

	var offset = (block_start - this.display_start) * this.width / display_length;
	this.canvas.setAttribute("transform", "translate("+offset+",0)");
    }
    this.zoomTo = zoomTo;

    function moveTo(time, align)
    {
	var block_start = context.getBlockStart();
	var display_length = context.getDisplayLength();
	var block_length = context.getBlockLength();
	if (align == null) align = 0.5;
	this.display_start = time  - display_length * align;
	if (this.display_start >= block_start
	    && ((this.display_start + display_length)
		<= (block_start + block_length))
	   && this.canvas != null) {
	    //console.log("Shift");
	} else {
	    block_start = (this.display_start
			   + (display_length - block_length)/2);
	    context.setBlockStart(block_start);
	    console.log((new Date(block_start))+" + "+this.block_length);
	    console.log("Update");
	    this.update();
	}
	//console.log("block_start: "+(new Date(block_start)));
	var offset = (block_start - this.display_start) * this.width / display_length;
	this.canvas.setAttribute("transform", "translate("+offset+",0)");
    }
    this.moveTo = moveTo;
    
    function update() 
    {
	var canvas =
	    document.createElementNS("http://www.w3.org/2000/svg","svg:g");
	context.setCanvas(canvas);
	var block_start = context.getBlockStart();
	var block_length = context.getBlockLength();
	console.log(block_start+"+"+block_length);
	for (var b in this.blocks) {
	    var block = this.blocks[b];
	    block.build_block(block_start,
			      block_start + block_length);
	}
	if (this.canvas == null) {
	    parent.appendChild(canvas);
	} else {
	    parent.replaceChild(canvas, this.canvas);
	}
	this.canvas = canvas;
    }
    this.update = update;

    function getContext()
    {
	return context;
    }
    this.getContext = getContext;
}

function StepCanvas(canvas, time_graph)
{
    var canvas = canvas;
    console.log(canvas);
    var time_graph = time_graph;
    
    function current() {
	return new Date(canvas.display_start
			+ canvas.getContext().getDisplayLength() / 2);
    }

    function forward(row)
    {
	var date = current();
	time_graph.next(row, date);
	canvas.moveTo(date.getTime(),0.5);
    }
    this.forward = forward;

    function backward(row)
    {
	var date = current();
	time_graph.prev(row, date);
	canvas.moveTo(date.getTime(), 0.5);
    }

    this.backward = backward;

    function msec(ms) 
    {
	var date = current();
	date.setMilliseconds(date.getMilliseconds()  + ms);
	canvas.moveTo(date.getTime(), 0.5);
    }
    this.msec = msec;

    function sec(s) 
    {
	var date = current();
	date.setSeconds(date.getSeconds()  + s);
	canvas.moveTo(date.getTime(), 0.5);
    }
    this.sec = sec;

    function day(d) 
    {
	var date = current();
	date.setDate(date.getDate()  + d);
	canvas.moveTo(date.getTime(), 0.5);
    }
    this.day = day;

    function month(m) 
    {
	var date = current();
	date.setMonth(date.getMonth()  + m);
	canvas.moveTo(date.getTime(), 0.5);
    }
    this.month = month;

    function year(y) 
    {
	var date = current();
	date.setFullYear(date.getFullYear()  + y);
	canvas.moveTo(date.getTime(), 0.5);
    }
    this.year = year;
    
    function zoom(scale)
    {
	var length = canvas.getContext().getDisplayLength()
	canvas.zoomTo(length * scale);
    }
    this.zoom = zoom;
}
    
function SignalGraphBlock(url, signal)
{
    TextGraphBlock.call(this,null);
    
    var parentSetContext = this.setContext;
    var parent_build_block = this.build_block;
    var wait_elem = null;
    var build_start;
    var build_end;
    var self = this;
    function setContext(c)
    {
	console.log("SignalGraphBlock.setContext");
	parentSetContext.call(this, c);
    }
    this.setContext = setContext;

    function reply(data )
    {
	
	self.getCanvas().removeChild(wait_elem);
	
	parent_build_block.call(self, build_start, build_end, []);
    }
    function error(req,status,error)
    {
	console.log("HTTP request failed: "+status+": "+error);
    } 
    function build_block(time_start, time_end)
    {
	block=document.createElementNS("http://www.w3.org/2000/svg","svg:rect");
	block.setAttribute("x", 0);
	block.setAttribute("y", this.getYPos());
	block.setAttribute("width", this.getBlockWidth());
	block.setAttribute("height", this.getBlockHeight());
	block.setAttribute("style", "fill:red;stroke:none");
	this.getCanvas().appendChild(block);
	wait_elem = block;
	build_start = time_start;
	build_end = time_end;
	var req_url = (url+"?start="+time_start+"&end="+time_end
		       +"&signals="+signal);
	jQuery.getJSON(req_url, reply).error(error);
    }
    this.build_block = build_block;
}

SignalGraphBlock.prototype = new TextGraphBlock();
SignalGraphBlock.prototype.constructor = SignalGraphBlock;

