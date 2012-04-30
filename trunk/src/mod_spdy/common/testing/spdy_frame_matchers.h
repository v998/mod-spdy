// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MOD_SPDY_TESTING_SPDY_FRAME_MATCHERS_H_
#define MOD_SPDY_TESTING_SPDY_FRAME_MATCHERS_H_

#include <iostream>

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace mod_spdy {

namespace testing {

class IsControlFrameOfTypeMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  explicit IsControlFrameOfTypeMatcher(net::SpdyControlType type)
      : type_(type) {}
  virtual ~IsControlFrameOfTypeMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const net::SpdyControlType type_;
  DISALLOW_COPY_AND_ASSIGN(IsControlFrameOfTypeMatcher);
};

// Make a matcher that requires the argument to be a control frame of the given
// type.
inline ::testing::Matcher<const net::SpdyFrame&> IsControlFrameOfType(
    net::SpdyControlType type) {
  return ::testing::MakeMatcher(new IsControlFrameOfTypeMatcher(type));
}

class IsDataFrameMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  IsDataFrameMatcher() {}
  virtual ~IsDataFrameMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  DISALLOW_COPY_AND_ASSIGN(IsDataFrameMatcher);
};

// Make a matcher that requires the argument to be a DATA frame.
inline ::testing::Matcher<const net::SpdyFrame&> IsDataFrame() {
  return ::testing::MakeMatcher(new IsDataFrameMatcher);
}

class IsDataFrameWithMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  explicit IsDataFrameWithMatcher(base::StringPiece payload)
      : payload_(payload) {}
  virtual ~IsDataFrameWithMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const base::StringPiece payload_;
  DISALLOW_COPY_AND_ASSIGN(IsDataFrameWithMatcher);
};

// Make a matcher that requires the argument to be a DATA frame with the given
// data payload.
inline ::testing::Matcher<const net::SpdyFrame&> IsDataFrameWith(
    base::StringPiece payload) {
  return ::testing::MakeMatcher(new IsDataFrameWithMatcher(payload));
}

class IsRstStreamMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  explicit IsRstStreamMatcher(net::SpdyStatusCodes status) : status_(status) {}
  virtual ~IsRstStreamMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const net::SpdyStatusCodes status_;
  DISALLOW_COPY_AND_ASSIGN(IsRstStreamMatcher);
};

// Make a matcher that requires the argument to be a RST_STREAM frame with the
// given status code.
inline ::testing::Matcher<const net::SpdyFrame&> IsRstStream(
    net::SpdyStatusCodes status) {
  return ::testing::MakeMatcher(new IsRstStreamMatcher(status));
}

class IsWindowUpdateMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  explicit IsWindowUpdateMatcher(uint32 delta) : delta_(delta) {}
  virtual ~IsWindowUpdateMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const uint32 delta_;
  DISALLOW_COPY_AND_ASSIGN(IsWindowUpdateMatcher);
};

// Make a matcher that requires the argument to be a WINDOW_UPDATE frame with
// the given window-size-delta.
inline ::testing::Matcher<const net::SpdyFrame&> IsWindowUpdate(uint32 delta) {
  return ::testing::MakeMatcher(new IsWindowUpdateMatcher(delta));
}

class FlagFinIsMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  FlagFinIsMatcher(bool fin) : fin_(fin) {}
  virtual ~FlagFinIsMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const bool fin_;
  DISALLOW_COPY_AND_ASSIGN(FlagFinIsMatcher);
};

// Make a matcher that requires the frame to have the given FLAG_FIN value.
inline ::testing::Matcher<const net::SpdyFrame&> FlagFinIs(bool fin) {
  return ::testing::MakeMatcher(new FlagFinIsMatcher(fin));
}

class StreamIdIsMatcher :
      public ::testing::MatcherInterface<const net::SpdyFrame&> {
 public:
  StreamIdIsMatcher(net::SpdyStreamId stream_id) : stream_id_(stream_id) {}
  virtual ~StreamIdIsMatcher() {}
  virtual bool MatchAndExplain(const net::SpdyFrame& frame,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(std::ostream* out) const;
  virtual void DescribeNegationTo(std::ostream* out) const;
 private:
  const net::SpdyStreamId stream_id_;
  DISALLOW_COPY_AND_ASSIGN(StreamIdIsMatcher);
};

// Make a matcher that requires the frame to have the given FLAG_FIN value.
inline ::testing::Matcher<const net::SpdyFrame&> StreamIdIs(bool fin) {
  return ::testing::MakeMatcher(new StreamIdIsMatcher(fin));
}

}  // namespace testing

}  // namespace mod_spdy

#endif  // MOD_SPDY_TESTING_SPDY_FRAME_MATCHERS_H_
