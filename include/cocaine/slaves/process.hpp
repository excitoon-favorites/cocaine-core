//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_SLAVE_PROCESS_HPP
#define COCAINE_SLAVE_PROCESS_HPP

#include "cocaine/slaves/base.hpp"

namespace cocaine { namespace engine { namespace slave {

class process_t:
    public slave_t
{
    public:
        process_t(engine_t* engine,
                  const std::string& type,
                  const std::string& args);

        virtual void reap();

    private:
        void signal(ev::child&, int);

    private:
        pid_t m_pid;
        ev::child m_child_watcher;
};

}}}

#endif