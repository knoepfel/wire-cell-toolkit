// Configure wire-cell per issue54
// track depos -> sim -> flow -> HDF


local wc = import "wirecell.jsonnet";
local g = import "pgraph.jsonnet";
local f = import "pgrapher/common/funcs.jsonnet";

local io = import "pgrapher/common/fileio.jsonnet";
local params = import "pgrapher/experiment/pdsp/simparams.jsonnet";
local tools_maker = import "pgrapher/common/tools.jsonnet";

local tools = tools_maker(params);

local sim_maker = import "pgrapher/experiment/pdsp/sim.jsonnet";
local sim = sim_maker(params, tools);

local close0 = {
    tail: wc.point(-3.000, 3.0, 1.000, wc.m),
    head: wc.point(-3.100, 3.0, 1.100, wc.m),
};
local tracklist = [
    {
        time: 0*wc.ms,
        charge: -5000,
        ray: close0,
    }
];
local depos = sim.tracks(tracklist);

// fixme: add depo flow output eventually
// local deposio = io.numpy.depos(output);
local drifter = sim.drifter;
local bagger = sim.make_bagger();
local sn_pipes = sim.splusn_pipelines;
local sn_graph = f.fanpipe('DepoSetFanout', sn_pipes, 'FrameFanin', "sn");
local f2t = g.pnode({type:"TaggedFrameTensorSet",data:{
    tensors:[{
        tag: "",
    }]}}, nin=1, nout=1);
local sink = g.pnode({
    type:"ZioTensorSetSink",
    data: {
        verbose: 1,
        timeout: 5000,
        // these are added to the flow object
        attributes: {           
            stream: "simsn",
        },
        connects: [
            { nodename: "issue54server", portname: "flow" }
        ]
    }
}, nin=1, nout=0);

local graph = g.pipeline([depos, drifter, bagger, sn_graph, f2t, sink]);

local app = {
    type: "Pgrapher",
    data: {
        edges: g.edges(graph),
    },
};

local cmdline = {
    type: "wire-cell",
    data: {
        plugins: ["WireCellGen", "WireCellPgraph", "WireCellSio",
                  "WireCellSigProc", "WireCellAux", "WireCellZio"],
        apps: ["Pgrapher"]
    }
};

local wccfg = [cmdline] + g.uses(graph) + [app];

/// above is normal wire-cell jsonnet, next comes the object for the zio flow file server

local ziocfg = [
    {
        rule: "(= direction 'inject')",
        rw: "w",
        filepat: "issue54.hdf",
        grouppat: "issue54/{stream}",
    },
    {
        rule: "(= direction 'extract')",
        rw: "r",
        filepat: "issue54.hdf",
        grouppat: "issue54/{stream}",
    }
];


{
    "wccfg.json": wccfg,
    "ziocfg.json": ziocfg,
}
