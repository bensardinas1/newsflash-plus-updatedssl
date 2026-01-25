// Copyright (c) 2010-2015 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// This software is copyrighted software. Unauthorized hacking, cracking, distribution
// and general assing around is prohibited.
// Redistribution and use in source and binary forms, with or without modification,
// without permission are prohibited.
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "newsflash/config.h"

#include <exception>
#include <memory>
#include <atomic>
#include <string>

#include "logging.h"

namespace newsflash
{
    // Work to be done in some thread where the thread could
    // be the application main (GUI) thread or some worker thread.
    class ThreadTask
    {
    public:
        enum class Affinity {
            GuiThread,

            // dispatch the task to any available thread
            // this means that multiple tasks from the same originating
            // object may complete in any order.
            AnyThread,

            // dispatch the task to a single thread with fixed affinity based
            // on the owner ID. this means that all tasks with SingleThread
            // affinity and with the owner ID will execute in the same thread.
            SingleThread
        };

        virtual ~ThreadTask() = default;

        explicit ThreadTask(Affinity a = Affinity::AnyThread) : owner_(0), affinity_(a)
        {
            static std::atomic<std::size_t> id(1);
            id_ = id++;
        }

        // Return whether an exception happened in perform()
        bool HasException() const noexcept
        {
            return exptr_ != std::exception_ptr();
        }

        // Rethrow the captured exception (if any)
        void rethrow() const
        {
            if (exptr_)
                std::rethrow_exception(exptr_);
        }

        // perform the action
        void PerformTask()
        {
            if (!logger_)
            {
                logger_  = std::make_shared<BufferLogger>();
            }

            Logger* prev = SetThreadLog(logger_.get());
            try
            {
                DoWork();
            }
            catch (const std::exception& e)
            {
                exptr_ = std::current_exception();
                LOG_E(e.what());
                if (logger_)
                    logger_->Flush();
            }
            SetThreadLog(prev);
        }

        // if the action has any completion callbacks run them now.
        // this is a separate function from perform() so that
        // it's possible that the action is executed on a separate
        // thread and when it's done the completion callbacks are invoked
        // on another (main/gui/engine) thread.
        virtual void run_completion_callbacks()
        {}

        // Get the human-readable description of the task.
        virtual std::string Describe() const
        { return {}; }

        virtual std::size_t size() const
        { return 0; }

        Affinity GetAffinity() const
        { return affinity_; }

        // get action object id.
        std::size_t GetOwnerId() const
        { return owner_; }

        std::size_t GetTaskId() const
        { return id_; }

        // set the owner id of the action.
        void SetOwnerId(std::size_t id)
        { owner_ = id;}

        // set the thread affinity.
        void SetAffinity(Affinity aff)
        { affinity_ = aff; }

        // set the logger object to be used for this action.
        void SetLogger(std::shared_ptr<Logger> out)
        { logger_ = std::move(out); }

        bool HasBufferedLogger() const
        {
            if (dynamic_cast<BufferLogger*>(logger_.get()))
                return true;
            return false;
        }
        const Logger* GetLogger() const
        {
            return logger_.get();
        }

    protected:
        virtual void DoWork() = 0;

    private:
        std::exception_ptr exptr_;
        std::size_t owner_ = 0;
        std::size_t id_    = 0;
        std::shared_ptr<Logger> logger_;
    private:
        Affinity affinity_ = Affinity::AnyThread;
    };

} // newsflash
