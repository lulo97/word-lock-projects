// Tests opt into the in-memory mock; npm start uses the real login item API.
const useMock = process.env.WORDLOCK_USE_MOCK_AUTO_START === '1';

module.exports = useMock ? require('./autoStart.mock') : require('./autoStart');
