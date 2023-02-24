/**
 * Copyright (c) 2017-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

module.exports = {
  title: "Profilo",
  tagline: "An Android performance library",
  url: "https://facebookincubator.github.io",
  baseUrl: "/profilo/",
  favicon: "img/favicon.png",
  organizationName: "facebookincubator", // Usually your GitHub org/user name.
  projectName: "profilo", // Usually your repo name.
  themeConfig: {
    algolia: {
      apiKey: '2d1e716ad64b24b64f695105790461a5',
      indexName: 'profilo',
    },
    navbar: {
      title: "Profilo",
      logo: {
        alt: "Profilo Logo",
        src: "img/profilo_logo_small.png"
      },
      links: [
        { to: "docs/getting-started", label: "Docs", position: "left" },
        {
          href: "https://github.com/facebookincubator/profilo",
          label: "GitHub",
          position: "right"
        }
      ]
    },
    footer: {
      style: "dark",
      links: [
        {
          title: 'Legal',
          items: [
            {
              label: 'Privacy',
              href: 'https://opensource.fb.com/legal/privacy/',
              target: '_blank',
              rel: 'noreferrer noopener',
            },
            {
              label: 'Terms',
              href: 'https://opensource.fb.com/legal/terms/',
              target: '_blank',
              rel: 'noreferrer noopener',
            },
          ],
        },
      ],
      logo: {
        alt: 'Meta Open Source Logo',
        src: 'https://docusaurus.io/img/meta_opensource_logo_negative.svg',
        href: 'https://opensource.fb.com',
      },
      copyright: `Copyright \u00A9 ${new Date().getFullYear()} Meta Platforms, Inc. Built with Docusaurus.`
    }
  },
  presets: [
    [
      "@docusaurus/preset-classic",
      {
        docs: {
          path: "../docs",
          sidebarPath: require.resolve("./sidebars.js")
        },
        theme: {
          customCss: require.resolve("./src/css/custom.css")
        }
      }
    ]
  ]
};
