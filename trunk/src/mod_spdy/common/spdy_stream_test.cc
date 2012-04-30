// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mod_spdy/common/spdy_stream.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "mod_spdy/common/spdy_frame_priority_queue.h"
#include "mod_spdy/common/testing/async_task_runner.h"
#include "mod_spdy/common/testing/notification.h"
#include "mod_spdy/common/testing/spdy_frame_matchers.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using mod_spdy::testing::FlagFinIs;
using mod_spdy::testing::IsDataFrameWith;
using mod_spdy::testing::IsRstStream;
using testing::AllOf;

namespace {

const net::SpdyStreamId kStreamId = 1;
const net::SpdyStreamId kAssocStreamId = 0;
const net::SpdyPriority kPriority = 2;

// Expect to get a frame from the queue (within 100 milliseconds) that is a
// data frame with the given payload and FLAG_FIN setting.
void ExpectDataFrame(mod_spdy::SpdyFramePriorityQueue* output_queue,
                     base::StringPiece data, bool flag_fin) {
  net::SpdyFrame* raw_frame;
  ASSERT_TRUE(output_queue->BlockingPop(
      base::TimeDelta::FromMilliseconds(100), &raw_frame));
  scoped_ptr<net::SpdyFrame> frame(raw_frame);
  EXPECT_THAT(*frame, AllOf(IsDataFrameWith(data), FlagFinIs(flag_fin)));
}

// Expect to get a frame from the queue (within 100 milliseconds) that is a
// RST_STREAM frame with the given status code.
void ExpectRstStream(mod_spdy::SpdyFramePriorityQueue* output_queue,
                     net::SpdyStatusCodes status) {
  net::SpdyFrame* raw_frame;
  ASSERT_TRUE(output_queue->BlockingPop(
      base::TimeDelta::FromMilliseconds(100), &raw_frame));
  scoped_ptr<net::SpdyFrame> frame(raw_frame);
  EXPECT_THAT(*frame, IsRstStream(status));
}

// When run, a SendDataTask sends the given data to the given stream.
class SendDataTask : public mod_spdy::testing::AsyncTaskRunner::Task {
 public:
  SendDataTask(mod_spdy::SpdyStream* stream, base::StringPiece data,
               bool flag_fin)
      : stream_(stream), data_(data), flag_fin_(flag_fin) {}
  virtual void Run() {
    stream_->SendOutputDataFrame(data_, flag_fin_);
  }
 private:
  mod_spdy::SpdyStream* const stream_;
  const base::StringPiece data_;
  const bool flag_fin_;
  DISALLOW_COPY_AND_ASSIGN(SendDataTask);
};

// Test that the flow control features are disabled for SPDY v2.
TEST(SpdyStreamTest, NoFlowControlInSpdy2) {
  net::BufferedSpdyFramer framer(2);
  mod_spdy::SpdyFramePriorityQueue output_queue;
  const int32 initial_window_size = 10;
  mod_spdy::SpdyStream stream(kStreamId, kAssocStreamId, kPriority,
                              initial_window_size, &output_queue, &framer);

  // Send more data than can fit in the initial window size.
  const base::StringPiece data = "abcdefghijklmnopqrstuvwxyz";
  stream.SendOutputDataFrame(data, true);

  // We should get all the data out in one frame anyway, because we're using
  // SPDY v2 and the stream shouldn't be using flow control.
  ExpectDataFrame(&output_queue, data, true);
  EXPECT_TRUE(output_queue.IsEmpty());
}

// Test that flow control works correctly for SPDY v3.
TEST(SpdyStreamTest, HasFlowControlInSpdy3) {
  net::BufferedSpdyFramer framer(3);
  mod_spdy::SpdyFramePriorityQueue output_queue;
  const int32 initial_window_size = 10;
  mod_spdy::SpdyStream stream(kStreamId, kAssocStreamId, kPriority,
                              initial_window_size, &output_queue, &framer);

  // Send more data than can fit in the initial window size.
  const base::StringPiece data = "abcdefghijklmnopqrstuvwxyz";
  mod_spdy::testing::AsyncTaskRunner runner(
      new SendDataTask(&stream, data, true));
  ASSERT_TRUE(runner.Start());

  // We should get a single frame out with the first initial_window_size=10
  // bytes (and no FLAG_FIN yet), and then the task should be blocked for now.
  ExpectDataFrame(&output_queue, "abcdefghij", false);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();

  // After increasing the window size by eight, we should get eight more bytes,
  // and then we should still be blocked.
  stream.AdjustWindowSize(8);
  ExpectDataFrame(&output_queue, "klmnopqr", false);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();

  // Finally, we increase the window size by fifteen.  We should get the last
  // eight bytes of data out (with FLAG_FIN now set), the task should be
  // completed, and the remaining window size should be seven.
  stream.AdjustWindowSize(15);
  ExpectDataFrame(&output_queue, "stuvwxyz", true);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectSetWithinMillis(100);
  EXPECT_EQ(7, stream.current_window_size());
}

// Test that flow control is well-behaved when the stream is aborted.
TEST(SpdyStreamTest, FlowControlAbort) {
  net::BufferedSpdyFramer framer(3);
  mod_spdy::SpdyFramePriorityQueue output_queue;
  const int32 initial_window_size = 7;
  mod_spdy::SpdyStream stream(kStreamId, kAssocStreamId, kPriority,
                              initial_window_size, &output_queue, &framer);

  // Send more data than can fit in the initial window size.
  const base::StringPiece data = "abcdefghijklmnopqrstuvwxyz";
  mod_spdy::testing::AsyncTaskRunner runner(
      new SendDataTask(&stream, data, true));
  ASSERT_TRUE(runner.Start());

  // We should get a single frame out with the first initial_window_size=7
  // bytes (and no FLAG_FIN yet), and then the task should be blocked for now.
  ExpectDataFrame(&output_queue, "abcdefg", false);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();
  EXPECT_FALSE(stream.is_aborted());

  // We now abort with a RST_STREAM frame.  We should get the RST_STREAM frame
  // out, but no more data, and the call to SendOutputDataFrame should return
  // even though the rest of the data was never sent.
  stream.AbortWithRstStream(net::PROTOCOL_ERROR);
  EXPECT_TRUE(stream.is_aborted());
  ExpectRstStream(&output_queue, net::PROTOCOL_ERROR);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectSetWithinMillis(100);

  // Now that we're aborted, any attempt to send more frames should be ignored.
  stream.SendOutputDataFrame("foobar", false);
  net::SpdyHeaderBlock headers;
  headers["x-foo"] = "bar";
  stream.SendOutputHeaders(headers, true);
  EXPECT_TRUE(output_queue.IsEmpty());
}

// Test that we abort the stream with FLOW_CONTROL_ERROR if the client
// incorrectly overflows the 31-bit window size value.
TEST(SpdyStreamTest, FlowControlOverflow) {
  net::BufferedSpdyFramer framer(3);
  mod_spdy::SpdyFramePriorityQueue output_queue;
  mod_spdy::SpdyStream stream(kStreamId, kAssocStreamId, kPriority, 0x60000000,
                              &output_queue, &framer);

  // Increase the window size so large that it overflows.  We should get a
  // RST_STREAM frame and the stream should be aborted.
  EXPECT_FALSE(stream.is_aborted());
  stream.AdjustWindowSize(0x20000000);
  EXPECT_TRUE(stream.is_aborted());
  ExpectRstStream(&output_queue, net::FLOW_CONTROL_ERROR);
  EXPECT_TRUE(output_queue.IsEmpty());
}

// Test that flow control works correctly even if the window size is
// temporarily negative.
TEST(SpdyStreamTest, NegativeWindowSize) {
  net::BufferedSpdyFramer framer(3);
  mod_spdy::SpdyFramePriorityQueue output_queue;
  const int32 initial_window_size = 10;
  mod_spdy::SpdyStream stream(kStreamId, kAssocStreamId, kPriority,
                              initial_window_size, &output_queue, &framer);

  // Send more data than can fit in the initial window size.
  const base::StringPiece data = "abcdefghijklmnopqrstuvwxyz";
  mod_spdy::testing::AsyncTaskRunner runner(
      new SendDataTask(&stream, data, true));
  ASSERT_TRUE(runner.Start());

  // We should get a single frame out with the first initial_window_size=10
  // bytes (and no FLAG_FIN yet), and then the task should be blocked for now.
  ExpectDataFrame(&output_queue, "abcdefghij", false);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();
  EXPECT_EQ(0, stream.current_window_size());

  // Adjust the window size down (as if due to a SETTINGS frame reducing the
  // initial window size).  Our current window size should now be negative, and
  // we should still be blocked.
  stream.AdjustWindowSize(-5);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();
  EXPECT_EQ(-5, stream.current_window_size());

  // Adjust the initial window size up, but not enough to be positive.  We
  // should still be blocked.
  stream.AdjustWindowSize(4);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();
  EXPECT_EQ(-1, stream.current_window_size());

  // Adjust the initial window size up again.  Now we should get a few more
  // bytes out.
  stream.AdjustWindowSize(4);
  ExpectDataFrame(&output_queue, "klm", false);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectNotSet();
  EXPECT_EQ(0, stream.current_window_size());

  // Finally, open the floodgates; we should get the rest of the data.
  stream.AdjustWindowSize(800);
  ExpectDataFrame(&output_queue, "nopqrstuvwxyz", true);
  EXPECT_TRUE(output_queue.IsEmpty());
  runner.notification()->ExpectSetWithinMillis(100);
  EXPECT_EQ(787, stream.current_window_size());
}

}  // namespace
