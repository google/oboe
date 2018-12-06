# Changelog

**This changelog is deprecated**. See the [Oboe releases page](https://github.com/google/oboe/releases) for the contents of each release. 

## [1.0.0](https://github.com/google/oboe/releases/tag/1.0.0)
#### 2nd October 2018
First production ready version

**API changes**
- [Remove `AudioStream::setNativeFormat`](https://github.com/google/oboe/pull/213/commits/0e8af6a65efef55ec180f8ce76e699adcee5f413)
- [Remove `AudioStream::isPlaying`](https://github.com/google/oboe/pull/213/commits/6437f5aa224330fbdf77ecc161cc868be663a974).
- [Add `AudioStream::getTimestamp(clockid_t)`](https://github.com/google/oboe/pull/213/commits/ab695c116e5f196e57560a86efa3c982360838d3).
- Deprecate `AudioStream::getTimestamp(clockid_t, int64_t, int64_t)`. Same commit as above.
- [Add Android P functions](https://github.com/google/oboe/commit/c30bbe603c256f92cdf2876c3122bc5be24b5e3e)

**Other changes**
- Add [API reference](https://google.github.io/oboe/)
- Add unit tests

## 0.11
#### 13th June 2018
Change `AudioStream` method return types to `ResultWithValue` where appropriate. [Full details](https://github.com/google/oboe/pull/109).

## 0.10
#### 18th January 2018
Add support for input (recording) streams

## 0.9
#### 18th October 2017
Initial developer preview
