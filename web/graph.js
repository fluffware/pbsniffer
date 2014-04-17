function setCookie(cname,cvalue,exdays)
{
    var expires = "";
    if (exdays) {
	var d = new Date();
	d.setDate(d.getDate()+exdays);
	expires = ";expires="+d.toGMTString();
    }
    document.cookie = cname + "=" + cvalue + expires;
    c = getCookies();
    c[cname] = cvalue;
} 

var cookies = null;
function getCookies()
{
    if (cookies == null) {
	cookies=[];
	var ca = document.cookie.split(';');
	for(var i=0; i<ca.length; i++) {
	    var c = ca[i].trim();
	    var ei = c.indexOf("=");
	    if (ei >= 0) {
		cookies[c.substring(0,ei)] = c.substring(ei+1);
	    }
	}
    }
    return cookies;
}

function ms_to_ns(ms)
{
    return ms * 1e6;
}

function ns_to_ms(ms)
{
    return ms * 1e-6;
}
const MIN_WIDTH = 3;
const SVG_NS = "http://www.w3.org/2000/svg";
function BlockContext(block_start, block_length, block_width)
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
	return block_width;
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
    
    function timeToXPos(t) 
    {
	return ((t - block_start) * block_width / block_length);
    }
    this.timeToXPos = timeToXPos;

    function xPosToTime(x) 
    {
	return (x * block_length / block_width + block_start);
    }
    this.xPosToTime = xPosToTime;

}

function GraphBlock(context)
{
    var block_height = 20;

    function build_block(time_start, time_end)
    {
	throw "build_block not implemented, implement in subclass";
    }
    this.build_block = build_block;
    
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
    
    function getBlockLength() {
	return context.getBlockLength();
    }
    this.getBlockLength = getBlockLength;
    
    
    function timeToXPos(t) 
    {
	return context.timeToXPos(t);
    }
    this.timeToXPos = timeToXPos;

    function xPosToTime(x) 
    {
	return context.xPosToTime(x);
    }
    this.xPosToTime = xPosToTime;
    
    function setContext(ctxt)
    {
	context = ctxt;
    }
    this.setContext = setContext;

    function snap(time) {
	return -1;
    }
    this.snap = snap;

    // x,y (in canvas coordinates) is inside this block for purpose of selection
    function isInside(x,y) {
	return false;
    }
    this.isInside = isInside;
}

function TextGraphBlock(context)
{
    GraphBlock.call(this,context);
    this.text_width = 50;
    var values = [];
    function build_block(parent,time_start, time_end, ts_values)
    {
	values = ts_values;
	var y_pos = 0;
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
	    var block=document.createElementNS("http://www.w3.org/2000/svg","svg:rect");
	    block.setAttribute("x", x);
	    block.setAttribute("y", y_pos);
	    block.setAttribute("width", width);
	    block.setAttribute("height", block_height);
	    block.setAttribute("class", "graph-block");
	    parent.appendChild(block);

	    for(var tx = x;
		tx == x || tx < end_x - text_repeat + 1; tx += text_repeat) {
		var text = document.createElementNS("http://www.w3.org/2000/svg",
						    "svg:text");
		text.setAttribute("style", "font-size:"+(block_height*0.8)+"px;dominant-baseline:central");
		text.setAttribute("class", "graph-text");
		text.setAttribute("x", tx+1);
		text.setAttribute("y", y_pos + block_height / 2);
	    var content = document.createTextNode(value);
		text.appendChild(content);
		parent.appendChild(text);
	    }
	}
    }
    this.build_block = build_block;

    function isInside(x,y) {
	return ((y >= 0)
		&& y < this.getBlockHeight());
    }
    this.isInside = isInside;

    function snap(time) {
	var diff = 10 * this.getBlockLength() / this.getBlockWidth();
	var ts = -1;
	for(var i = 0; i < values.length; i+= 2) {
	    if (Math.abs(values[i] - time) < diff) {
		diff = Math.abs(values[i] - time);
		ts = values[i];
	    }
	}
	return ts;
    }
    this.snap = snap;

    function drawTextLabel(parent, x,y, w,h, str)
    {
	var text = document.createElementNS("http://www.w3.org/2000/svg",
					    "svg:text");
	text.setAttribute("style", "font-size:"+(h*0.8)+"px;dominant-baseline:central");
	text.setAttribute("class", "label-text");
	text.setAttribute("x", x+1);
	text.setAttribute("y", y + h/2);
	var content = document.createTextNode(str);
	text.appendChild(content);
	parent.appendChild(text);
    }
    this.drawTextLabel = drawTextLabel;
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
	return date.getUTCMilliseconds();
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
	return date.getUTCMilliseconds();
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
	return date.getUTCMilliseconds();
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
	return date.getUTCSeconds();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
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
	return date.getUTCSeconds();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(trunc(date.getUTCSeconds(),10));
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
	return date.getUTCMinutes();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(0);
    }
    this.next = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() + 1);
    }
    this.prev = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() - 1);
    }
}

function Format_10m()
{
    this.label = "m";
    this.format = function(date) {
	return date.getUTCMinutes();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(0);
	date.setUTCMinutes(trunc(date.getUTCMinutes(),10));
    }
    this.next = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() + 10);
    }
    this.prev = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() - 10);
    }
}

function Format_h()
{
    this.label = "h";
    this.format = function(date) {
	return date.getUTCHours();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(0);
	date.setUTCMinutes(0);
    }
    this.next = function(date) {
	date.setUTCHours(date.getUTCHours() + 1);
    }
    this.prev = function(date) {
	date.setUTCHours(date.getUTCHours() - 1);
    }
}

function Format_8h()
{
    this.label = "h";
    this.format = function(date) {
	return date.getUTCHours();
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(0);
	date.setUTCMinutes(0);
	date.setUTCHours(trunc(date.getUTCHours(), 8));
    }
    this.next = function(date) {
	date.setUTCHours(date.getUTCHours() + 8);
    }
    this.prev = function(date) {
	date.setUTCHours(date.getUTCHours() - 8);
    }
}

function Format_D()
{
    this.label = "D";
    this.format = function(date) {
	return date.getUTCDate();
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
    }
    this.next = function(date) {
	date.setUTCDate(date.getUTCDate() + 1);
    }
    this.prev = function(date) {
	date.setUTCDate(date.getUTCDate() - 1);
    }
}

function Format_10D()
{
    this.label = "D";
    this.format = function(date) {
	return date.getUTCDate();
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
	date.setUTCDate(trunc(date.getUTCDate(),10));

    }
    this.next = function(date) {
	date.setUTCDate(date.getUTCDate() + 10);
    }
    this.prev = function(date) {
	date.setUTCDate(date.getUTCDate() - 10);
    }
}

function Format_M()
{
    this.label = "M";
    this.format = function(date) {
	return date.getUTCMonth()+1;
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
	date.setUTCDate(0);
    }
    this.next = function(date) {
	date.setUTCMonth(date.getUTCMonth() + 1);
    }
    this.prev = function(date) {
	date.setUTCMonth(date.getUTCMonth() - 1);
    }
}

function Format_Y()
{
    this.label = "Y";
    this.format = function(date) {
	return date.getUTCFullYear();
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
	date.setUTCDate(0);
	date.setUTCMonth(0);
    }
    this.next = function(date) {
	date.setUTCFullYear(date.getUTCFullYear() + 1);
    }
    this.prev = function(date) {
	date.setUTCFullYear(date.getUTCFullYear() - 1);
    }
}


function padToTwo(n) {
    return ("0"+n).slice(-2);
}

function Format_hms()
{
    this.label = "h:m:s";
    this.format = function(date) {
	return date.getUTCHours()+":"+padToTwo(date.getUTCMinutes())+":"+padToTwo(date.getUTCSeconds());
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
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
    this.label = "h:m";
    this.format = function(date) {
	return date.getUTCHours()+":"+padToTwo(date.getUTCMinutes());
    }
    this.first = function(date) {
	date.setUTCMilliseconds(0);
	date.setUTCSeconds(0);
    }
    this.next = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() + 1);
    }
    this.prev = function(date) {
	date.setUTCMinutes(date.getUTCMinutes() - 1);
    }
}


function Format_YMD()
{
    this.label = "YYYY-MM-DD";
    this.format = function(date) {
	return date.getUTCFullYear()+"-"+(date.getUTCMonth()+1)+"-"+date.getUTCDate();
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
    }
    this.next = function(date) {
	date.setUTCDate(date.getUTCDate() + 1);
    }
    this.prev = function(date) {
	date.setUTCDate(date.getUTCDate() - 1);
    }
}

function Format_YM()
{
    this.label = "YYYY-MM";
    this.format = function(date) {
	return date.getUTCFullYear()+"-"+(date.getUTCMonth()+1);
    }
    this.first = function(date) {
	date.setUTCHours(0,0,0,0);
	date.setUTCDate(0);
    }
    this.next = function(date) {
	date.setUTCMonth(date.getUTCMonth() + 1);
    }
    this.prev = function(date) {
	date.setUTCMonth(date.getUTCMonth() - 1);
    }
}


/* min_span is in milliseconds */
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

    function build_block(parent,time_start, time_end)
    {
	var sub_height = this.getBlockHeight() / 3;
	for (s in sub_blocks) {
	    sub_blocks[s].setBlockHeight(sub_height);
	}
	
	var y_pos = 0;
	var graph_width = this.getBlockWidth();
	var block_height = this.getBlockHeight();
	
	var min_text_width = 40;
	
	var start_date = new Date(ns_to_ms(time_start));
	var end_date = new Date(ns_to_ms(time_end))
	
	var format = null;
	var block_time = (time_end - time_start) * min_text_width / this.getBlockWidth();;

	for (var f in formats) {
	    format = formats[f];
	    if (ms_to_ns(format.min_span) > block_time) break;
	}
	if (format.block_objects == null) {
	    format.block_objects = [];
	    for (var b in format.block_classes) {
		format.block_objects[b]= new format.block_classes[b]();
	    }
	}
	format.block_objects[0].first(start_date);
	for (var b in format.block_objects) {
	    var date = new Date(ns_to_ms(time_start));
	    var values = [];
	    var block = format.block_objects[b];
	    block.first(date)
	    var t;
	    while ((t = ms_to_ns(date.getTime())) < time_end) {
		values.push(t);
		values.push(block.format(date));
		block.next(date)
	    }
	    var trans = document.createElementNS("http://www.w3.org/2000/svg",
						 "svg:g");
	    trans.setAttribute("transform", "translate(0, "
			       + (b * sub_height) + ")");
	    sub_blocks[b].build_block(trans, time_start, time_end, values);
	    parent.appendChild(trans);
	}
	current_format = format;

    }
    this.build_block = build_block;

    function drawLabel(parent, x,y, w,h)
    {
	if (current_format == null) return;
	for (var f in current_format.block_objects) {
	    var bf = current_format.block_objects[f];
	    sub_blocks[f].drawTextLabel(parent, x+1,y+f*h/3,w, h/3, bf.label);
	}
    }
    this.drawLabel = drawLabel;

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
    
    function isInside(x,y) {
	return ((y >= 0)
		&& (y < this.getBlockHeight()));
    }
    this.isInside = isInside;

    function snap(time) {
	var diff = 10 * this.getBlockLength() / this.getBlockWidth();
	var ts = -1;
	for (var s in sub_blocks) {
	    var t = sub_blocks[s].snap(time);
	    if (t >= 0 && Math.abs(t - time) < diff) {
		diff = Math.abs(t - time);
		ts = t;
	    }
	}
	return ts;
    }
    this.snap = snap;
}

TimeGraphBlock.prototype = new GraphBlock();
TimeGraphBlock.prototype.constructor = TimeGraphBlock;

function TimeCursor(id, context) 
{
    GraphBlock.call(this,context);
    var cursor_time = null;
    var cursor = null;
    this.moved = $.Callbacks();
    var cb_add = this.moved.add;
    this.moved.add =
	function(cb) {cb_add.call(this, cb); this.fire(cursor_time);};

    function build_block(parent, time_start, time_end)
    {
	if (cursor_time == null) cursor_time = (time_start + time_end) / 2;
	cursor =document.createElementNS("http://www.w3.org/2000/svg","svg:line");
	cursor.setAttribute("y1", 0);
	cursor.setAttribute("y2", this.getBlockHeight());
	cursor.setAttribute("class", "time-cursor");
	cursor.setAttribute("id", "cursor-"+id);
	this.setCursorTime(cursor_time);
	parent.appendChild(cursor);
    }
    this.build_block = build_block;
    
    function setCursorTime(time) {
	cursor_time = time;
	var x = this.timeToXPos(cursor_time);
	cursor.setAttribute("x1", x);
	cursor.setAttribute("x2", x);
	this.moved.fire(cursor_time);
    }
    this.setCursorTime = setCursorTime;

    function getCursorTime() {
	return cursor_time;
    }
    this.getCursorTime = getCursorTime;
}
TimeCursor.prototype = new GraphBlock();
TimeCursor.prototype.constructor = TimeCursor;

function TimeCanvas(parent, pw, ph, label_parent, lw)
{
    this.width = pw;
    var context = new BlockContext(0,1000, pw*3);

    this.display_start = 0;
    this.display_length = 1e6;
    this.display_width = pw;
    this.canvas = null;
    this.labels_canvas = null;
    this.blocks = [];
    var self = this;

    $(parent).mousedown(mouse_down);
    this.canvas = document.createElementNS(SVG_NS,"svg:g");
    parent.appendChild(this.canvas);

    function getBlockAt(x,y)
    {
	if (y < 0) return null;
	for (b in this.blocks) {
	    if (this.blocks[b].obj.isInside(x,y-this.blocks[b].y_pos))
		return this.blocks[b].obj;
	}
	return null;
    }
    this.getBlockAt = getBlockAt;

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
	if (event.pageX == drag_x && event.pageY == drag_y) {
	    
	} else {
	    var delta_x = event.pageX-drag_x;
	    var delta_time =
		-self.display_length * delta_x / self.display_width;
	    self.moveTo(self.display_start+delta_time, 0);
	}

	drag_x = event.pageX;
	drag_y = event.pageY;
	event.preventDefault();
    }

    function mouse_move(event)
    {
	event.preventDefault();
    }

    function setDisplayLength(l)
    {
	this.display_length = l;
    }
    this.setDisplayLength = setDisplayLength;

    function getDisplayLength()
    {
	return this.display_length;
    }
     this.getDisplayLength = getDisplayLength;

    function setBlockLength(l)
    {
	context.setBlockLength(l);
    }
    this.setBlockLength = setBlockLength;
    
    function findBlockObj(obj)
    {
	for (b in self.blocks) {
	    if (self.blocks[b].obj == obj) {
		return self.blocks[b];
	    }
	}
	return null;
    }

    function findBlockObjIndex(obj)
    {
	for (b in self.blocks) {
	    if (self.blocks[b].obj == obj) {
		return b;
	    }
	}
	return -1;
    }

    function setYPos(block_obj,y)
    {
	var block = findBlockObj(block_obj);
	if (block) {
	    block.y_pos = y;
	    update_block(block);
	}
    } 
    this.setYPos = setYPos;

    function addBlock(block_obj, y, before)
    {
	block_obj.setContext(context);
	if (before) {
	    var b = findBlockObjIndex(before);
	    console.log("Adding block at "+b);
	    this.blocks.splice(b, 0, {obj:block_obj, y_pos:y});
	} else {
	    this.blocks.push({obj:block_obj, y_pos:y});
	}
    }

    this.addBlock = addBlock;

    function removeBlock(block)
    {
	var b = findBlockObjIndex(block);
	if (b >= 0) {
	    console.log("Removing "+b);
	    if (this.blocks[b].trans) {
		this.canvas.removeChild(this.blocks[b].trans);
	    }
	    this.blocks.splice(b,1);
	}
    }

    this.removeBlock = removeBlock;

    
    function zoomTo(display_length)
    {
	var mid_display = this.display_start + this.display_length / 2;
	this.display_start = mid_display - display_length / 2;
	this.display_length = display_length;
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
	var display_length = this.display_length;
	var block_length = context.getBlockLength();
	if (align == null) align = 0.5;
	this.display_start = time  - display_length * align;
	if (this.display_start >= block_start
	    && ((this.display_start + display_length)
		<= (block_start + block_length))
	   && this.canvas != null) {
	    //console.log("Shift");
	    var offset = ((block_start - this.display_start) * this.width
			  / display_length);
	    this.canvas.setAttribute("transform", "translate("+offset+",0)");
	} else {
	    block_start = (this.display_start
			   + (display_length - block_length)/2);
	    context.setBlockStart(block_start);
	    //console.log((new Date(ns_to_ms(block_start)))+" + "+this.block_length);
//	    console.log("Update");
	    this.update();
	}
//	console.log("block_start: "+(new Date(ns_to_ms(block_start))));
    }
    this.moveTo = moveTo;
    
    function update_block(block)
    {
	var block_start = context.getBlockStart();
	var block_length = context.getBlockLength();
	
	var trans = document.createElementNS(SVG_NS, "svg:g");
	trans.setAttribute("transform", "translate(0,"+(block.y_pos)+")");
	block.obj.build_block(trans, block_start,
			      block_start + block_length);
	if (block.trans) {
	    self.canvas.replaceChild(trans, block.trans);
	} else {
	    self.canvas.appendChild(trans);
	}
	block.trans = trans;
    }

    function update() 
    {
	var block_start = context.getBlockStart();
	var block_length = context.getBlockLength();
	for (var b in this.blocks) {
	    var block = this.blocks[b];
	    update_block(block);
	    this.canvas.appendChild(block.trans);
	}
	
	var offset = ((block_start - this.display_start) * this.display_width
		      / this.display_length);
	this.canvas.setAttribute("transform", "translate("+offset+",0)");
	
	if (label_parent) {
	    var canvas =
		document.createElementNS("http://www.w3.org/2000/svg","svg:g");
	    for (var b in this.blocks) {
		var block = this.blocks[b];
		if (block.obj.drawLabel) {
		    block.obj.drawLabel(canvas,0,block.y_pos,
					lw, block.obj.getBlockHeight());
		}
	    }
	    if (this.label_canvas == null) {
		label_parent.appendChild(canvas);
	    } else {
		label_parent.replaceChild(canvas, this.label_canvas);
	    }
	    this.label_canvas = canvas;

	}
    }
    this.update = update;

    function getContext()
    {
	return context;
    }
    this.getContext = getContext;

    function screenPosToBlockPos(x,y) {
	var pt = $(parent).closest("svg\\:svg").get(0).createSVGPoint();
	pt.x = x;
	pt.y = y;	
	return pt.matrixTransform(self.canvas.getScreenCTM().inverse());
    }

    function screenPosToTime(x,y) {
	return self.context.xPosToTime(screenPosToBlockPos(x,y).x);
    }
    this.screenPosToTime = screenPosToTime;

    function screenPosToSnappedTime(x,y) {
	var blockpt = screenPosToBlockPos(x,y);
	var time = context.xPosToTime(screenPosToBlockPos(x,y).x);
	var selected = self.getBlockAt(blockpt.x, blockpt.y);
	if (selected) {
	    var t = selected.snap(time);
	    if (t >= 0) time = t;
	}
	return time;
    }
    this.screenPosToSnappedTime = screenPosToSnappedTime;
}

function StepCanvas(canvas, time_graph)
{
    var canvas = canvas;
    var time_graph = time_graph;
    
    function current() {
	return new Date(ns_to_ms(canvas.display_start
				 + canvas.getDisplayLength() / 2));
    }

    function forward(row)
    {
	var date = current();
	time_graph.next(row, date);
	canvas.moveTo(ms_to_ns(date.getTime()),0.5);
    }
    this.forward = forward;

    function backward(row)
    {
	var date = current();
	time_graph.prev(row, date);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }

    this.backward = backward;

    function msec(ms) 
    {
	var date = current();
	date.setUTCMilliseconds(date.getUTCMilliseconds()  + ms);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }
    this.msec = msec;

    function sec(s) 
    {
	var date = current();
	date.setUTCSeconds(date.getUTCSeconds()  + s);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }
    this.sec = sec;

    function day(d) 
    {
	var date = current();
	date.setUTCDate(date.getUTCDate()  + d);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }
    this.day = day;

    function month(m) 
    {
	var date = current();
	date.setUTCMonth(date.getUTCMonth()  + m);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }
    this.month = month;

    function year(y) 
    {
	var date = current();
	date.setUTCFullYear(date.getUTCFullYear()  + y);
	canvas.moveTo(ms_to_ns(date.getTime()), 0.5);
    }
    this.year = year;

    function page(p) 
    {
	canvas.moveTo(canvas.display_start
		      + canvas.getDisplayLength() * p, 0);
    }
    this.page = page;
    
    function zoom(scale)
    {
	var length = canvas.getDisplayLength()
	canvas.zoomTo(length * scale);
    }
    this.zoom = zoom;

    function goTo(t) {
	canvas.moveTo(t, 0.5);
    }
    this.goTo = goTo;
}
    
function SignalGraphBlock(url, signal)
{
    TextGraphBlock.call(this,null);
    
    var parentSetContext = this.setContext;
    var parent_build_block = this.build_block;
    var label = signal;
    var wait_elem = null;
    var build_start;
    var build_end;
    var build_parent;

    var label_parent = null;
    var label_x;
    var label_y;
    var label_w;
    var label_h;

    var self = this;
    function setContext(c)
    {
	parentSetContext.call(this, c);
    }
    this.setContext = setContext;

    function reply(data )
    {
	if (wait_elem) {
	    build_parent.removeChild(wait_elem);
	    wait_elem = null;
	}
	if (data.error != null) {
	    console.log("Request error: "+data.error);
	} else {
	    if (data[signal]) {
		if (data[signal].label) {
		    label = data[signal].label;
		}
		parent_build_block.call(self, build_parent, build_start, build_end, data[signal].values);
		if (label_parent) {
		    self.drawTextLabel(label_parent, label_x, label_y,
				       label_w, label_h, label);
		    label_parent = null;
		}
	    }
	}
    }
    function error(req,status,error)
    {
	console.log("HTTP request failed: "+status+": "+error);
    } 
    function build_block(parent,time_start, time_end)
    {
	block=document.createElementNS("http://www.w3.org/2000/svg","svg:rect");
	block.setAttribute("x", 0);
	block.setAttribute("y", 0);
	block.setAttribute("width", this.getBlockWidth());
	block.setAttribute("height", this.getBlockHeight());
	block.setAttribute("style", "fill:red;stroke:none");
	parent.appendChild(block);
	wait_elem = block;
	build_parent = parent;
	build_start = time_start;
	build_end = time_end;
	var req_url = (url+"?start="+Math.round(time_start)
		       +"&end="+Math.round(time_end)
		       +"&signals="+signal
		       +"&min_duration="
		       +Math.round(MIN_WIDTH * this.getBlockLength()
				   / this.getBlockWidth()));
	jQuery.getJSON(req_url, reply).error(error);
    }
    this.build_block = build_block;

    function drawLabel(parent, x,y, w,h)
    {
	label_parent = parent;
	label_x = x;
	label_y = y;
	label_w = w;
	label_h = h;
    }
    this.drawLabel = drawLabel;
}

SignalGraphBlock.prototype = new TextGraphBlock();
SignalGraphBlock.prototype.constructor = SignalGraphBlock;

var next_id = 1;

function SignalViewer(id, log_url)
{
    var time_height = 60;
    var signal_height = 20;

    var graph_width = 700;
    var graph_height = time_height;

    var signals = [];
    var cursors = {};
    var active_cursor = null;
    var secondary_cursor = null;
   
    var label_width = 150;

    var ns = "http://www.w3.org/2000/svg";
    var svg = document.createElementNS(ns,"svg:svg");

    // Graph area
    var graph_clip = document.createElementNS(ns,"svg:clipPath");
    var graph_clip_id = "clip_"+next_id++;
    graph_clip.setAttribute("id", graph_clip_id);
    svg.appendChild(graph_clip);
    var graph_clip_rect = document.createElementNS(ns,"svg:rect");
    graph_clip.appendChild(graph_clip_rect);
    var graph = document.createElementNS(ns,"svg:g");
    graph.setAttribute("clip-path", "url(#"+graph_clip_id+")");
    svg.appendChild(graph);
    var graph_bg = document.createElementNS(ns,"svg:rect");
    graph_bg.setAttribute("class", "graph-bg");
    graph.appendChild(graph_bg);

    // Label area
    var label_clip = document.createElementNS(ns,"svg:clipPath");
    var label_clip_id = "clip_"+next_id++;
    label_clip.setAttribute("id", label_clip_id);
    svg.appendChild(label_clip);
    label_clip_rect = document.createElementNS(ns,"svg:rect");
    label_clip.appendChild(label_clip_rect);
    var label = document.createElementNS(ns,"svg:g");
    label.setAttribute("transform", "translate("+graph_width+","+0+")");
    label.setAttribute("clip-path", "url(#"+label_clip_id+")");
    svg.appendChild(label);
    var label_bg = document.createElementNS(ns,"svg:rect");
    label_bg.setAttribute("class", "label-bg");
    label.appendChild(label_bg);


    var tc = new TimeCanvas(graph, graph_width, graph_height,
			    label, label_width);
    /* Timeline */
    var tb = new TimeGraphBlock();
    tb.setBlockHeight(time_height);
    tc.addBlock(tb, graph_height - time_height);

   
    var display_length =60e9;  // 60 sec
    var buffer_factor = 3;

    var now = new Date();
    tc.setDisplayLength(display_length);
    tc.setBlockLength(display_length * buffer_factor);
    set_sizes();

    tc.moveTo(ms_to_ns(now.getTime()))

    $(graph).click(mouse_click);

    function set_sizes()
    {
	graph_height = time_height;
	var y = 0;
	for (s in signals) {
	    var h = signals[s].getBlockHeight();
	    graph_height += h;
	    tc.setYPos(signals[s],y);
	    y += h;
	}
	tc.setYPos(tb,graph_height - time_height);
	for (c in cursors) {
	    cursors[c].setBlockHeight(graph_height);
	}
	svg.setAttribute("width", graph_width + label_width);
	svg.setAttribute("height", graph_height);

	set_rect_size(graph_clip_rect, 0,0, graph_width, graph_height);
	set_rect_size(graph_bg, 0,0, graph_width, graph_height);

	set_rect_size(label_clip_rect, 0,0, label_width, graph_height);
	set_rect_size(label_bg, 0,0, label_width, graph_height);
    }

    function set_rect_size(e, x,y,w,h)
    {
	e.setAttribute("x", x);
	e.setAttribute("y", y);
	e.setAttribute("width", w);
	e.setAttribute("height", h);	
    }

    function mouse_click(event)
    {
	var time = tc.screenPosToSnappedTime(event.clientX, event.clientY);
	console.log("Snap "+ (new Date(ns_to_ms(time))));
	if (event.shiftKey) {
	    if (secondary_cursor) {
		cursors[secondary_cursor].setCursorTime(time);
	    }
	} else {
	    if (active_cursor) {
		cursors[active_cursor].setCursorTime(time);
	    }
	}
	event.preventDefault();
    }

    function clearSignals()
    {
	for (s in signals) {
	    tc.removeBlock(signals[s]);
	}
	signals = [];
	set_sizes();
	tc.update();
    }

    this.clearSignals = clearSignals;

    function addSignal(id, options)
    {
	var s = new SignalGraphBlock(log_url, id);
	s.setBlockHeight(signal_height);
	tc.addBlock(s, 0, tb);
	signals.push(s);
	set_sizes();
	tc.update();
    }
    this.addSignal = addSignal;
    
    function addCursor(id)
    {
	var cursor =new TimeCursor(id);
	cursors[id] = cursor;
	if (active_cursor == null) active_cursor = id;
	tc.addBlock(cursor,0);
	set_sizes();
	tc.update();
	return cursor;
    }

    this.addCursor = addCursor;

    function getCursor(id)
    {
	return cursors[id];
    }

    this.getCursor = getCursor;

    function setActiveCursor(id)
    {
	active_cursor = id;
    }
    this.setActiveCursor = setActiveCursor;

    function setSecondaryCursor(id)
    {
	secondary_cursor = id;
    }
    this.setSecondaryCursor = setSecondaryCursor;

    
    function moveToCursor(id)
    {
	if (cursors[id]) {
	    var t = cursors[id].getCursorTime();
	    tc.moveTo(t);
	}
    }
    this.moveToCursor = moveToCursor;

    var stepper = new StepCanvas(tc, tb);
    function getStepper()
    {
	return stepper;
    }
    this.getStepper = getStepper;
    
    function getTop()
    {
	return svg;
    }
    this.getTop = getTop;

}

function move_signal_row(node)
{ 
    var table = node.closest("table");
    var table_id = table.attr("id");
    console.log("Table id: "+table_id);
    var row = node.closest("tr");
    var new_table;
    if (table_id == "signals_shown") {
	new_table = $("#signals_available");
    } else {
	new_table = $("#signals_shown");
    }
    row.addClass("moved");
    new_table.append(row);

}

function update_signals(viewer)
{
    viewer.clearSignals();
    var signal_ids = [];
    $("#signals_shown tr[id]").each(function() {
	var signal = this.getAttribute("id").substring(7); // Remove "signal_"
	viewer.addSignal(signal);
	signal_ids.push(signal);
    });

    $("#signals_shown .moved").removeClass("moved");
    $("#signals_available .moved").removeClass("moved");
    setCookie("signals", signal_ids.join(","));
}

function display_cursor_time(t, node) {
    var d = new Date(ns_to_ms(t));
    var dstr = (d.getUTCFullYear()+"-"+(d.getUTCMonth()+1)+"-"
		+d.getUTCDate()+" "
		+d.getUTCHours()+":"+padToTwo(d.getUTCMinutes())
		+":"+padToTwo(d.getUTCSeconds())
		+"."+("00000"+Math.round(t / 1000) % 1000000).slice(-6));
    $(node).each(function () {
	this.innerHTML=dstr;
    });
}
