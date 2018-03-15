/**
 * Copyright (c) 2017-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/* List of projects/orgs using your project for the users page */
const users = [
];

const siteConfig = {
  title: 'Profilo' /* title for your website */,
  tagline: 'An Android performance library',
  url: 'https://facebookincubator.github.io' /* your website url */,
  baseUrl: '/profilo/' /* base url for your project */,
  projectName: 'profilo',
  organizationName: "facebookincubator",
  headerLinks: [
    {doc: 'getting-started', label: 'Docs'},
    {
      href: 'https://github.com/facebookincubator/profilo',
      label: 'GitHub',
    },
  ],
  users,
  /* path to images for header/footer */
  headerIcon: 'img/profilo_logo_small.png',
  footerIcon: 'img/profilo_logo_small.png',
  favicon: 'img/favicon.png',
  /* colors for website */
  colors: {
    primaryColor: '#351312',
    secondaryColor: '#cf352d',
  },
  /* custom fonts for website */
  /*fonts: {
    myFont: [
      "Times New Roman",
      "Serif"
    ],
    myOtherFont: [
      "-apple-system",
      "system-ui"
    ]
  },*/
  // This copyright info is used in /core/Footer.js and blog rss/atom feeds.
  copyright:
    'Copyright © ' +
    new Date().getFullYear() +
    ' Facebook Inc.',
  // organizationName: 'deltice', // or set an env variable ORGANIZATION_NAME
  // projectName: 'test-site', // or set an env variable PROJECT_NAME
  highlight: {
    // Highlight.js theme to use for syntax highlighting in code blocks
    theme: 'default',
  },
  scripts: ['https://buttons.github.io/buttons.js'],
  // You may provide arbitrary config keys to be used as needed by your template.
  repoUrl: 'https://github.com/facebookincubator/profilo',
  /* On page navigation for the current documentation page */
  // onPageNav: 'separate',
};

module.exports = siteConfig;
