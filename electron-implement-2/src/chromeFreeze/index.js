// Tests opt into the in-memory mock; npm start always uses the real Windows API.
const useMock = process.env.WORDLOCK_CHROME_FREEZE_MOCK === '1';

module.exports = useMock ? require('./chromeFreeze.mock') : require('./chromeFreeze');
