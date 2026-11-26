const { defineConfig } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './tests',
  timeout: 30_000,
  fullyParallel: false,
  workers: 1,
  reporter: 'list',
  use: {
    // Tests always use the mock; npm start uses the real ntdll implementation.
    launchOptions: {
      env: { ...process.env, WORDLOCK_CHROME_FREEZE_MOCK: '1' },
    },
  },
});
