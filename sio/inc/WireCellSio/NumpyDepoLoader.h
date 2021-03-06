/** Load depos from a Numpy file. */

#ifndef WIRECELLSIO_NUMPYDEPOLOADER
#define WIRECELLSIO_NUMPYDEPOLOADER

#include "WireCellIface/IDepoSource.h"
#include "WireCellIface/IConfigurable.h"

#include <deque>

namespace WireCell {
    namespace Sio {

        // This loader is the counterpart to NumpyDepoSaver.
        class NumpyDepoLoader : public WireCell::IDepoSource, public WireCell::IConfigurable {
           public:
            NumpyDepoLoader();
            virtual ~NumpyDepoLoader();

            /// IDepoSource.  This faithfully reproduces the EOS
            /// behavior followed by NumpyDepoSaver when it wrote the
            /// file.
            virtual bool operator()(IDepo::pointer& out);

            /// IConfigurable
            virtual WireCell::Configuration default_configuration() const;
            virtual void configure(const WireCell::Configuration& config);

           private:
            // try to load next array
            bool next();

            Configuration m_cfg;
            int m_load_count;  // count frames loaded
            int m_eos;         // keep track if outside a stream

            // buffer one frame of depos
            std::deque<WireCell::IDepo::pointer> m_depos;
        };

    }  // namespace Sio
}  // namespace WireCell
#endif
