#include "WireCellAux/TaggedFrameTensorSet.h"
#include "WireCellUtil/NamedFactory.h"
#include "WireCellAux/SimpleTensor.h"
#include "WireCellAux/SimpleTensorSet.h"
#include "WireCellIface/FrameTools.h"
#include "WireCellUtil/Logging.h"

#include<iostream>

WIRECELL_FACTORY(TaggedFrameTensorSet, WireCell::Aux::TaggedFrameTensorSet,
                 WireCell::IFrameTensorSet, WireCell::IConfigurable)

using namespace WireCell;

WireCell::Configuration Aux::TaggedFrameTensorSet::default_configuration() const
{
    // Each tag will produce a tensor.
    Configuration cfg;
    cfg["tensors"] = Json::arrayValue;
    return cfg;
}

void Aux::TaggedFrameTensorSet::configure(const WireCell::Configuration& config)
{
    m_cfg = config;
}

bool Aux::TaggedFrameTensorSet::operator()(const input_pointer& in, output_pointer& out)
{
    out = nullptr;
    if (!in) {                  // pass on EOS
        return true;
    }
    
    auto log = Log::logger("tens");

    ITensor::vector* itv = new ITensor::vector;

    Configuration md;
    md["time"] = in->time();
    md["tick"] = in->tick();

    for (auto jten : m_cfg["tensors"]) {
        auto tag = get<std::string>(jten, "tag", "");
        jten["tag"] = tag;      // assure it is there for output
        auto traces = FrameTools::tagged_traces(in, tag);
        if (traces.empty()) {
            log->warn("Frame->Tensor: no traces for tag '{}', skipping", tag);
            continue;
        }
        size_t ntraces = traces.size();
        //log->debug("Frame->Tensor: {} traces for tag '{}'", ntraces, tag);

        float pad = get<float>(jten, "pad", 0.0);
        jten["pad"] = pad;

        auto mm_tbin = FrameTools::tbin_range(traces);
        int tbin0 = mm_tbin.first;
        jten["tbin"] = tbin0;

        // traces may be degenerate/overlapping in channels and ticks.
        // Do a little dance to map chid to an index sorted by chid.
        std::vector<int> uchid;
        for (const auto trace : traces) {
            uchid.push_back(trace->channel());
        }
        //log->debug("Frame->Tensor: tag '{}': {} channels total, {} traces", tag, uchid.size(), ntraces);

        std::sort(uchid.begin(), uchid.end());
        std::vector<int>::iterator it = std::unique(uchid.begin(), uchid.end());
        const size_t nchans = std::distance(uchid.begin(), it);
        //log->debug("Frame->Tensor: tag '{}': {} channels unique", tag, nchans);
        uchid.resize(nchans);

        // map channel ID to consecutive, sorted index
        std::map<int,size_t> chid2ind;
        for (auto chid : uchid) {
            size_t ind = chid2ind.size();
            chid2ind[chid] = ind;
        }

        // "channels" tensor
        SimpleTensor<int>* cht = new SimpleTensor<int>({nchans});
        Eigen::Map<Eigen::ArrayXi> charr((int*)cht->data(), nchans);
        assert((size_t)charr.size() == nchans);
        charr.setZero();

        // "summary" tensor, create but only fill and save if we have summary
        SimpleTensor<double>* sumt = new SimpleTensor<double>({nchans});
        Eigen::Map<Eigen::ArrayXd> sumarr((double*)sumt->data(), nchans);
        assert((size_t)sumarr.size() == nchans);
        sumarr.setZero();

        const auto& ts = in->trace_summary(tag);
        bool have_summary = ts.size() > 0;

        size_t nticks = mm_tbin.second - mm_tbin.first;
        const std::vector<size_t> shape = {nchans, nticks};

        // "waveform" tensor
        SimpleTensor<float>* st = new SimpleTensor<float>(shape);
        Eigen::Map<Eigen::ArrayXXf> arr((float*)st->data(), nchans, nticks);
        arr.setConstant(pad);

        for (size_t itr=0; itr<ntraces; ++itr) {
            auto trace = traces[itr];
            const auto& qarr = trace->charge();
            size_t nq = qarr.size();

            int chid = trace->channel();
            auto it = chid2ind.find(chid);
            assert(it != chid2ind.end());
            size_t chind = it->second;
            assert(chind < (size_t)charr.size());

            //log->debug("itr: {}/{}, chid: {}, chind: {}", itr, ntraces, chid, chind);
            charr[chind] = chid;
            if (have_summary) {
                sumarr[chind] += ts[itr];
            }

            size_t tbini = trace->tbin() - tbin0; // where in output to lay-down charge.
            size_t tbinf = std::min(nticks, tbini + nq);
            size_t tbinn = tbinf - tbini;
            
            Eigen::Map<const Eigen::ArrayXf> wave(qarr.data(), nq);
            arr.row(chind).segment(tbini, tbinn) += wave;
        }

        jten["waveform"] = (int)itv->size();
        itv->push_back(ITensor::pointer(st));

        jten["channels"] = (int)itv->size();
        itv->push_back(ITensor::pointer(cht));

        if (have_summary) {
            jten["summary"] = (int)itv->size();            
            itv->push_back(ITensor::pointer(sumt));
        }

        md["tensors"].append(jten);

    }

    out = std::make_shared<SimpleTensorSet>(in->ident(), md,
                                            ITensor::shared_vector(itv));
    return true;
}





Aux::TaggedFrameTensorSet::TaggedFrameTensorSet()
{
}
Aux::TaggedFrameTensorSet::~TaggedFrameTensorSet()
{
}

