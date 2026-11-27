const { defineConfig } = require('@playwright/test');

// Everything runs against the packaged Neutralino binary with in-memory test
// data, mocked Chrome freeze and mocked auto-start.
process.env.WORDLOCK_USE_TEST_DATA = '1';
process.env.WORDLOCK_CHROME_FREEZE_MOCK = '1';
process.env.WORDLOCK_USE_MOCK_AUTO_START = '1';

module.exports = defineConfig({
  testDir: './tests',
  timeout: 60_000,
  expect: { timeout: 10_000 },
  fullyParallel: false,
  workers: 1,
  maxFailures: 1,
  reporter: 'list',
  use: {
    trace: 'retain-on-failure',
  },
});
