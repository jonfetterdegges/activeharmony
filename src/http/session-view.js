/*
 * Copyright 2003-2013 Jeffrey K. Hollingsworth
 *
 * This file is part of Active Harmony.
 *
 * Active Harmony is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Active Harmony is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Active Harmony.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Global Variables */
var appName;
var sig;
var trials = new Array();
var best;
var chartData;
var intervalHandle;
var timezoneOffset;

function shutdown_comm(xhr, status, error)
{
    if (intervalHandle)
        clearInterval(intervalHandle);

    $("#app_status").html(status);
    $("#sess_ctl_div :input").attr("disabled", true);
    $("#interval").attr("disabled", true);
}

// Effectively, the main() routine for this program.
$(document).ready(function(){
    ajaxSetup(shutdown_comm);

    var d = new Date();
    timezoneOffset = d.getTimezoneOffset() * 60 * 1000;

    var s_idx = document.URL.indexOf("?");
    appName = document.URL.slice(s_idx + 1);
    $("#app_name").html(appName);

    requestInit(appName, handleInitData);
});

function handleInitData(data)
{
    var pairs = data.split("|");

    for (var i = 0; i < pairs.length; ++i) {
        var pair = keyval(pairs[i]);

        switch (pair.key) {
        case "sig":
            updateSignature(pair.val);
            break;
        case "strat":
            $("#app_strategy").html(pair.val);
            break;
        default:
            alert("Protocol error: Static data: " + pairs[i]);
            break;
        }
    }

    chartData = new Array();
    for (var i = 0; i < sig.varNames.length + 1; ++i)
        chartData[i] = new Array();
    refresh();

    updateTableHeader();
    updateViewList();
    updatePlotSize();
    updateInterval();
    $("#table_div").css("display", "inherit");
}

function updateSignature(sig_string)
{
    sig = new Object();
    sig.varNames = new Array();
    sig.varTypes = new Array();
    sig.setVals  = new Array();

    var ranges = sig_string.split(",");
    for (var i = 0; i < ranges.length; ++i) {
        var arr = ranges[i].split(";");
        var name = arr.shift();
        var type = arr.shift();

        sig.varNames[i] = name;
        sig.varTypes[i] = type;
        if (type == "s")
            sig.setVals[i] = arr;
    }
}

function updateTableHeader()
{
    var cell = $("#table_head");
    var names = sig.varNames.concat(["Performance"]);

    for (var i = 0; i < names.length; ++i) {
        if (cell.is(":last-child"))
            cell.after("<th></th>");
        cell = cell.next("th");
        cell.html(names[i]);
    }
    cell.nextAll("th").remove();
}

function updateViewList()
{
    var list = $("#view_opts");

    for (var i = 0; i < sig.varNames.length; ++i) {
        if (list.is(":last-child"))
            list.after("<option></option>");
        list = list.next("option");
        list.text(sig.varNames[i]);
    }
    list.nextAll("option").remove();
}

function updatePlotSize()
{
    var dim = $("#chart_size").val().split("x");
    $("#plot_div").css("width",  dim[0] + "px");
    $("#plot_div").css("height", dim[1] + "px");
}

function updateInterval()
{
    var delay = $("#interval").val();

    if (intervalHandle)
        clearInterval(intervalHandle);

    intervalHandle = setInterval(refresh, delay);
}

function handleRefreshData(data)
{
    var pairs = data.split("|");
    var oldTrialsLength = trials.length;
    var index;

    /* mechanism only updates parts of the page that need to be updated
      (what was sent from the session with the session data request) */
    for (var i = 0; i < pairs.length; ++i) {
        var pair = keyval(pairs[i]);

        switch (pair.key) {
        case "time":
            $("#svr_time").html(fullDate(pair.val));
            break;
        case "status":
            $("#app_status").html("Active - " + pair.val);
            break;
        case "best":
            best = parseTrial("Best," + pair.val);
            redrawBest(true);
            break;
        case "clients":
            $("#app_clients").html(pair.val);
            break;
        case "index":
            index = parseInt(pair.val);
            break;
        case "trial":
            if (index == trials.length) {
                appendTrial(pair.val);
            }
            else if (index > trials.length) {
                alert("Timing error: Invalid index. (Got:" +
                      index + " Expecting:" + trials.length + ")");
            }
            ++index;
            break;

        default:
            alert("Protocol error: Refresh data: " + pairs[i]);
        }
    }

    if (oldTrialsLength != trials.length) {
        redrawTable();
        redrawChart();
    }
}

function appendTrial(trial_string)
{
    var trial = parseTrial(trial_string);

    var xvals = [trial["time"] - timezoneOffset].concat(trial);
    for (var i = 0; i < xvals.length; ++i) {
        var point = new Array();
        point[0] = xvals[i];
        point[1] = trial["perf"];
        point["trial"] = trial;

        chartData[i].push(point);
    }
    trials.push(trial);
}

function parseTrial(trial_string)
{
    var text = trial_string.split(",");
    var trial = new Array();

    trial["time"] = parseInt(text.shift());
    for (var i = 0; i < sig.varTypes.length; ++i) {
        switch (sig.varTypes[i]) {
        case "i":
        case "s": trial[i] = parseInt(text[i]); break;
        case "r": trial[i] = parseFloat(text[i]); break;
        }
    }
    trial["perf"] = parseFloat(text.splice(-1));

    return trial;
}

function redrawBest(highlight)
{
    var cell = $("#table_best");
    var vals = new Array();

    for (var i = 0; i < best.length; ++i)
        vals[i] = displayVal(best, i);
    vals.push(displayVal(best, "perf"));

    for (var i = 0; i < vals.length; ++i) {
        if (cell.is(":last-child"))
            cell.after("<td></td>");
        cell = cell.next("td");

        if (highlight) {
            if (cell.html() != vals[i])
                cell.css("background-color", "GreenYellow");
            else
                cell.css("background-color", "inherit");
        }
        cell.html(vals[i]);
    }
    cell.nextAll("td").remove();
}

function redrawTable()
{
    var numRows = $("#table_len").val();
    var valCols = sig.varNames.length;
    var tblHtml;

    var idx = trials.length - 1;
    for (var i = 0; i < numRows; ++i) {
        var trial = trials[idx];

        tblHtml += "<tr><td>" + displayVal(trial, "time") + "</td>";
        for (var j = 0; j < valCols; ++j)
            tblHtml += "<td>" + displayVal(trial, j) + "</td>";
        tblHtml += "<td>" + displayVal(trial, "perf") + "</td></tr>";

        if (--idx < 0)
            break;
    }
    $("#table_body").html(tblHtml);
}

function displayVal(trial, index)
{
    var type;
    var val = trial[index];

    switch (index) {
    case "time": type = "t"; break;
    case "perf": type = "r"; break;
    default:     type = sig.varTypes[index];
    }

    switch (type) {
    case "i": return val.toString();
    case "r": return val.toPrecision($("#precision").val());
    case "s": return '"' + sig.setVals[index][val] + '"';
    case "t": return timeDate(val);
    }
}

var prevPoint = null;
function redrawChart()
{
    var i = $("#view_list :selected").index();
    var x_mode = null;
    if (i == 0)
        x_mode = "time";

    $.plot($("#plot_div"), [chartData[i]], { xaxis:  {mode:x_mode},
                                             grid:   {hoverable:true},
                                             points: {show:true}});
    $("#plot_div").bind("plothover", function (event, pos, item) {
        if (item) {
            if (prevPoint != item.dataIndex) {
                if (prevPoint != null)
                    $("#popup_div").remove();
                prevPoint = item.dataIndex;

                var details = popupText(item.dataIndex);
                $("<div id='popup_div'>" + details + "</div>").css( {
                    position: 'absolute',
                    display: 'none',
                    top: item.pageY + 5,
                    left: item.pageX - 150,
                    border: '1px solid #fdd',
                    padding: '2px',
                    'background-color': '#fee',
                    opacity: 0.80
                }).appendTo("body").fadeIn(100);
            }
        }
        else {
            $("#popup_div").remove();
            prevPoint = null;
        }
    });
}

function popupText(pointIdx)
{
    var chartIdx = $("#view_list :selected").index();
    var trial = chartData[chartIdx][pointIdx]["trial"];

    var retval = "Timestamp:" + displayVal(trial, "time") + "<br>";
    for (var i = 0; i < sig.varNames.length; ++i)
        retval += sig.varNames[i] + ": " + displayVal(trial, i) + "<br>";
    retval += "Performance:" + displayVal(trial, "perf");

    return retval;
}

function refresh()
{
    requestRefresh(appName, trials.length, handleRefreshData);
}

function restart()
{
    requestRestart(appName, document.getElementById("init_point").value);
    setTimeout(refresh, 250);
}

function pause()
{
    requestPause(appName);
    setTimeout(refresh, 250);
}

function resume()
{
    requestResume(appName);
    setTimeout(refresh, 250);
}

function kill()
{
    requestKill(appName);
    setTimeout(refresh, 250);
}
